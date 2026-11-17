#include "ui/library.hpp"

#include <QClipboard>
#include <QGuiApplication>
#include <QStandardPaths>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace peerchalk {
namespace {

const std::array<QString, 4> verificationNames = {QStringLiteral("none"),
                                                  QStringLiteral("in-person"),
                                                  QStringLiteral("call"), QStringLiteral("shared")};

QString dataDirectory() {
    const QString overridden = qEnvironmentVariable("PEERCHALK_DATA_DIR");
    return overridden.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                                : overridden;
}

// The login name with a capital first letter, or "Me".
QString defaultDisplayName() {
    QString name = qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME")).trimmed();
    if (name.isEmpty())
        return QStringLiteral("Me");
    name[0] = name[0].toUpper();
    return name;
}

QUuid parseId(const QString &id) {
    const QUuid uuid = QUuid::fromString(id);
    if (uuid.isNull())
        throw std::invalid_argument("Unknown item");
    return uuid;
}

QByteArray parseKey(const QString &keyHex) {
    const QByteArray key = QByteArray::fromHex(keyHex.toLatin1());
    if (key.size() != signingKeySize)
        throw std::invalid_argument("Unknown person");
    return key;
}

QString idText(const QUuid &id) { return id.toString(QUuid::WithoutBraces); }

} // namespace

Library::Library(const QString &directory, QObject *parent)
    : QObject(parent), store_(directory), identity_(directory, defaultDisplayName()),
      projects_(new RowModel({"projectId", "name", "colorIndex", "boardCount"}, this)),
      boards_(new RowModel({"boardId", "name", "edited"}, this)),
      contacts_(new RowModel({"key", "name", "fingerprint", "verification"}, this)) {
    if (!store_.projects().empty())
        currentProject_ = store_.projects().front().id;
    refreshProjects();
    refreshBoards();
    refreshContacts();
}

Library *Library::create(QQmlEngine *, QJSEngine *) {
    try {
        return new Library(dataDirectory());
    } catch (const std::exception &error) {
        qFatal("PeerChalk cannot read its data: %s", error.what());
    }
}

bool Library::attempt(const std::function<void()> &action) {
    try {
        action();
        return true;
    } catch (const std::exception &error) {
        emit errorOccurred(QString::fromStdString(error.what()));
        return false;
    }
}

void Library::refreshProjects() {
    QList<QVariantMap> rows;
    for (const ProjectInfo &project : store_.projects())
        rows.append({{QStringLiteral("projectId"), idText(project.id)},
                     {QStringLiteral("name"), project.name},
                     {QStringLiteral("colorIndex"), project.colorIndex},
                     {QStringLiteral("boardCount"), static_cast<int>(project.boards.size())}});
    projects_->setRows(std::move(rows));
}

void Library::refreshBoards() {
    std::vector<BoardInfo> boards;
    if (!currentProject_.isNull())
        boards = store_.project(currentProject_).boards;
    std::ranges::sort(boards, std::greater{}, &BoardInfo::edited);
    QList<QVariantMap> rows;
    for (const BoardInfo &board : boards) {
        if (!board.name.contains(boardFilter_.trimmed(), Qt::CaseInsensitive))
            continue;
        rows.append({{QStringLiteral("boardId"), idText(board.id)},
                     {QStringLiteral("name"), board.name},
                     {QStringLiteral("edited"), board.edited}});
    }
    boards_->setRows(std::move(rows));
}

void Library::refreshContacts() {
    QList<QVariantMap> rows;
    for (const Contact &contact : identity_.contacts())
        rows.append({{QStringLiteral("key"), QString::fromLatin1(contact.signingKey.toHex())},
                     {QStringLiteral("name"), contact.name},
                     {QStringLiteral("fingerprint"), formatFingerprint(contact.signingKey)},
                     {QStringLiteral("verification"),
                      verificationNames.at(static_cast<std::size_t>(contact.verification))}});
    contacts_->setRows(std::move(rows));
}

QString Library::currentProjectId() const {
    return currentProject_.isNull() ? QString() : idText(currentProject_);
}

void Library::setCurrentProjectId(const QString &id) {
    attempt([&] {
        const QUuid project = store_.project(parseId(id)).id;
        if (project == currentProject_)
            return;
        currentProject_ = project;
        refreshBoards();
        emit currentProjectChanged();
    });
}

void Library::setBoardFilter(const QString &filter) {
    if (filter == boardFilter_)
        return;
    boardFilter_ = filter;
    refreshBoards();
    emit boardFilterChanged();
}

QString Library::currentProjectName() const {
    return currentProject_.isNull() ? QString() : store_.project(currentProject_).name;
}

void Library::setDisplayName(const QString &name) {
    if (attempt([&] { identity_.setDisplayName(name); }))
        emit profileChanged();
}

void Library::setColorIndex(int index) {
    if (attempt([&] { identity_.setColorIndex(index); }))
        emit profileChanged();
}

QString Library::fingerprint() const { return formatFingerprint(identity_.signingKey()); }

QString Library::contactCode() const { return encodeContactCard(identity_.contactCard()); }

void Library::copyToClipboard(const QString &text) { QGuiApplication::clipboard()->setText(text); }

QString Library::createProject(const QString &name) {
    QString id;
    attempt([&] {
        currentProject_ = store_.createProject(name).id;
        id = idText(currentProject_);
        refreshProjects();
        refreshBoards();
        emit currentProjectChanged();
    });
    return id;
}

QString Library::joinProject(const QString &code) {
    QString id;
    attempt([&] {
        const std::optional<Invite> invite = decodeInvite(code);
        if (!invite)
            throw std::invalid_argument("That is not a PeerChalk invite");
        currentProject_ = store_.joinProject(*invite).id;
        id = idText(currentProject_);
        refreshProjects();
        refreshBoards();
        emit currentProjectChanged();
    });
    return id;
}

QString Library::projectInvite(const QString &projectId) {
    QString code;
    attempt([&] {
        const ProjectInfo &project = store_.project(parseId(projectId));
        code = encodeInvite({project.id, project.name, project.capability});
    });
    return code;
}

void Library::renameProject(const QString &projectId, const QString &name) {
    if (attempt([&] { store_.renameProject(parseId(projectId), name); })) {
        refreshProjects();
        emit currentProjectChanged();
    }
}

void Library::removeProject(const QString &projectId) {
    attempt([&] {
        const QUuid id = parseId(projectId);
        store_.removeProject(id);
        if (id == currentProject_)
            currentProject_ = store_.projects().empty() ? QUuid() : store_.projects().front().id;
        refreshProjects();
        refreshBoards();
        emit currentProjectChanged();
    });
}

QString Library::createBoard(const QString &name) {
    QString id;
    attempt([&] {
        if (currentProject_.isNull())
            throw std::invalid_argument("Create or join a project first");
        id = idText(store_.createBoard(currentProject_, name).id);
        refreshProjects();
        refreshBoards();
    });
    return id;
}

void Library::renameBoard(const QString &boardId, const QString &name) {
    if (attempt([&] { store_.renameBoard(parseId(boardId), name); }))
        refreshBoards();
}

void Library::removeBoard(const QString &boardId) {
    if (attempt([&] { store_.removeBoard(parseId(boardId)); })) {
        refreshProjects();
        refreshBoards();
    }
}

QString Library::boardName(const QString &boardId) {
    QString name;
    attempt([&] { name = store_.board(parseId(boardId)).name; });
    return name;
}

QByteArray Library::boardState(const QString &boardId) {
    QByteArray state;
    attempt([&] { state = store_.boardState(parseId(boardId)); });
    return state;
}

void Library::saveBoardState(const QString &boardId, const QByteArray &state) {
    if (attempt([&] { store_.saveBoardState(parseId(boardId), state); }))
        refreshBoards();
}

bool Library::addContact(const QString &code) {
    const bool added = attempt([&] {
        const std::optional<ContactCard> card = decodeContactCard(code);
        if (!card)
            throw std::invalid_argument("That is not a PeerChalk contact code");
        identity_.addContact(*card);
    });
    if (added)
        refreshContacts();
    return added;
}

void Library::setVerification(const QString &keyHex, const QString &how) {
    const bool changed = attempt([&] {
        const auto it = std::ranges::find(verificationNames, how);
        if (it == verificationNames.end())
            throw std::invalid_argument("Unknown way of verifying");
        identity_.setVerification(parseKey(keyHex),
                                  static_cast<Verification>(it - verificationNames.begin()));
    });
    if (changed)
        refreshContacts();
}

void Library::removeContact(const QString &keyHex) {
    if (attempt([&] { identity_.removeContact(parseKey(keyHex)); }))
        refreshContacts();
}

} // namespace peerchalk
