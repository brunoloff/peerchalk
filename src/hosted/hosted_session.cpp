#include "hosted/hosted_session.hpp"

#include "hosted/hosted_account.hpp"
#include "repository/encoding.hpp"
#include "repository/state_vector.hpp"
#include "sync/network.hpp"
#include "sync/repository_scribe.hpp"
#include "ui/library.hpp"

#include <QJsonArray>

namespace peerchalk::hosted {
namespace {

constexpr int pollInterval = 50;
constexpr int hostedProtocol = 1;

void singleShot(QTimer &timer, int interval) {
    timer.setSingleShot(true);
    timer.setInterval(interval);
}

} // namespace

HostedSession::HostedSession(std::unique_ptr<HostedBridge> bridge, QString accountDirectory,
                             Timing timing, QObject *parent)
    : QObject(parent), bridge_(std::move(bridge)), directory_(std::move(accountDirectory)),
      timing_(timing) {
    if (!bridge_)
        throw std::invalid_argument("A hosted session needs a bridge to its dashboard");
    pollTimer_.setInterval(pollInterval);
    connect(&pollTimer_, &QTimer::timeout, this, &HostedSession::poll);
    singleShot(saveTimer_, timing_.saveDelay);
    singleShot(idleTimer_, timing_.idleDelay);
    singleShot(pushTimer_, timing_.pushInterval);
    connect(&saveTimer_, &QTimer::timeout, this, [this] { save(false); });
    const auto pushIfScribe = [this] {
        if (unpushed_ && network_ && network_->repository().isScribe(board_))
            save(true);
    };
    connect(&idleTimer_, &QTimer::timeout, this, pushIfScribe);
    connect(&pushTimer_, &QTimer::timeout, this, pushIfScribe);
}

void HostedSession::start() {
    bridge_->post(
        {{"type", "ready"}, {"protocol", hostedProtocol}, {"version", PEERCHALK_VERSION}});
    pollTimer_.start();
}

void HostedSession::poll() {
    for (const QJsonObject &message : bridge_->take())
        receive(message);
}

void HostedSession::receive(const QJsonObject &message) {
    const QString type = message["type"].toString();
    try {
        if (type == QStringLiteral("open"))
            open(message);
        else if (type == QStringLiteral("update"))
            update(message);
        else if (type == QStringLiteral("pushed"))
            pushed(message);
        else if (type == QStringLiteral("pushFailed"))
            fail(QStringLiteral("Pushing failed: ") + message["error"].toString());
        else if (type == QStringLiteral("close"))
            close();
        else
            fail(QStringLiteral("Unknown message from the dashboard: ") + type);
    } catch (const std::exception &error) {
        fail(QStringLiteral("Could not handle \"%1\": %2")
                 .arg(type, QString::fromUtf8(error.what())));
    }
}

void HostedSession::open(const QJsonObject &message) {
    if (open_)
        throw MessageError("a board is open already");
    OpenMessage parsed = parseOpen(message);
    loaded_ = prepareAccount(directory_, parsed);
    identity_ = std::make_unique<UserIdentity>(directory_);
    clients_.insert(parsed.clients.begin(), parsed.clients.end());
    canPush_ = parsed.canPush;
    board_ = parsed.board;
    open_ = std::move(parsed);
    for (const repository::LoadWarning &warning : loaded_.warnings)
        fail(warning.path + QStringLiteral(": ") + warning.reason);
    emit opened(board_);
}

void HostedSession::attach(Library &library, sync::Network &network) {
    library_ = &library;
    network_ = &network;
    connect(&library, &Library::boardChanged, this, &HostedSession::edited);
    connect(&network.repository(), &sync::RepositoryScribe::scribeChanged, this,
            [this](const QUuid &board, bool isScribe) {
                if (board == board_)
                    scribeChanged(isScribe);
            });
    clients_.insert(library.document(board_).board().clientId());
    network.repository().setLocal(board_, canPush_);
}

void HostedSession::update(const QJsonObject &message) {
    if (!open_ || !library_)
        throw MessageError("no board is open");
    const UpdateMessage parsed = parseUpdate(message);
    for (const auto &[path, bytes] : parsed.files) {
        if (bytes)
            open_->files[path] = *bytes;
        else
            open_->files.erase(path);
    }
    open_->access.insert(open_->access.end(), parsed.access.begin(), parsed.access.end());
    loaded_ = repository::loadBoard(open_->files, open_->access);
    std::vector<std::pair<QByteArray, QByteArray>> ops;
    for (const SignedOp &op : loaded_.ops)
        ops.emplace_back(op.payload, op.signature);
    library_->workspace().acceptOps(ops);
    library_->document(board_).board().apply(repository::byteView(loaded_.state));
    if (parsed.canPush && *parsed.canPush != canPush_) {
        canPush_ = *parsed.canPush;
        network_->repository().setLocal(board_, canPush_);
    }
}

void HostedSession::edited(const QUuid &board) {
    if (board != board_)
        return;
    unpushed_ = true;
    authors_.insert(library_->displayName());
    for (const QVariant &device : network_->boardPresence(board_.toString(QUuid::WithoutBraces)))
        authors_.insert(device.toMap()[QStringLiteral("name")].toString());
    saveTimer_.start();
    if (network_->repository().isScribe(board_)) {
        idleTimer_.start();
        if (!pushTimer_.isActive())
            pushTimer_.start();
    }
}

void HostedSession::scribeChanged(bool isScribe) {
    if (isScribe && unpushed_)
        save(true);
}

repository::FileChanges HostedSession::accessFiles() const {
    repository::FileChanges files;
    for (const SignedOp &op : library_->workspace().board(board_).ops)
        files[QStringLiteral("access/") + repository::accessFileName(op)] =
            repository::encodeAccessFile(op);
    return files;
}

QJsonArray HostedSession::authors() const {
    QJsonArray names;
    for (const QString &name : authors_)
        if (!name.isEmpty())
            names.append(QJsonObject{{"name", name}});
    return names;
}

void HostedSession::save(bool push) {
    const Board &document = library_->document(board_).board();
    const std::vector<std::uint64_t> clients(clients_.begin(), clients_.end());
    repository::FileChanges files = accessFiles();
    files[repository::changeFilePath(identity_->deviceKey())] =
        repository::changeFile(document, loaded_, repository::authorOf(*identity_), clients);
    QJsonObject encoded;
    for (const auto &[path, bytes] : files)
        encoded[path] = bytes ? bytesValue(*bytes) : QJsonValue();
    QJsonArray clientIds;
    for (const std::uint64_t client : clients)
        clientIds.append(QString::number(client));
    savedStates_[++revision_] = repository::toByteArray(document.stateVector());
    bridge_->post({{"type", "save"},
                   {"board", board_.toString(QUuid::WithoutBraces)},
                   {"revision", revision_},
                   {"files", encoded},
                   {"clients", clientIds},
                   {"push", push},
                   {"authors", authors()}});
    saveTimer_.stop();
    if (push) {
        idleTimer_.stop();
        pushTimer_.stop();
    }
}

void HostedSession::pushed(const QJsonObject &message) {
    const int revision = message["revision"].toInt();
    const auto state = savedStates_.find(revision);
    const QByteArray commit = QByteArray::fromHex(message["commit"].toString().toLatin1());
    if (state == savedStates_.end() || (commit.size() != 20 && commit.size() != 32))
        throw MessageError("pushed names an unknown revision or a malformed commit");
    network_->repository().announcePushed(board_, commit, state->second);
    savedStates_.erase(savedStates_.begin(), std::next(state));
    if (revision == revision_) {
        unpushed_ = false;
        authors_.clear();
    }
}

// Whether the latest push holds everything this device wrote, whoever pushed it.
bool HostedSession::ownEditsPushed() const {
    const std::optional<repository::StateVector> own = repository::decodeStateVector(
        repository::toByteArray(library_->document(board_).board().stateVector()));
    repository::StateVector mine;
    for (const std::uint64_t client : clients_)
        if (own && own->contains(client))
            mine[client] = own->at(client);
    if (mine.empty())
        return true;
    const std::optional<sync::RepoPushed> last = network_->repository().lastPushed(board_);
    const std::optional<repository::StateVector> pushed =
        last ? repository::decodeStateVector(last->stateVector) : std::nullopt;
    return pushed && repository::covers(*pushed, mine);
}

void HostedSession::close() {
    if (library_)
        save(!ownEditsPushed());
    bridge_->post({{"type", "closed"}});
}

void HostedSession::fail(const QString &text) {
    qWarning("PeerChalk hosted: %s", qPrintable(text));
    bridge_->post({{"type", "warning"}, {"text", text}});
}

} // namespace peerchalk::hosted
