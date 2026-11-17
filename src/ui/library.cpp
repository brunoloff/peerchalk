#include "ui/library.hpp"

#include <QClipboard>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTimer>

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

QString idText(const QUuid &id) { return id.toString(QUuid::WithoutBraces); }

} // namespace

Library::Library(const QString &directory, QObject *parent)
    : QObject(parent), identity_(directory, defaultDisplayName()),
      workspace_(directory, identity_.signingKey(), identity_.identity().signer()),
      saveTimer_(new QTimer(this)),
      projects_(new RowModel({"projectId", "name", "colorIndex", "boardCount", "admin", "pending"},
                             this)),
      boards_(new RowModel({"boardId", "name", "edited", "admin", "pending"}, this)),
      contacts_(new RowModel({"key", "name", "fingerprint", "verification"}, this)) {
    saveTimer_->setSingleShot(true);
    saveTimer_->setInterval(500);
    connect(saveTimer_, &QTimer::timeout, this, &Library::saveBoards);
    ensureValidSelection();
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

// Keeps the selection on an existing project, or else the first one, or "shared".
void Library::ensureValidSelection() {
    const auto visible = [](const ProjectEntry &p) { return !p.deleted(); };
    if (currentProject_ == sharedId())
        return;
    const QUuid current = QUuid::fromString(currentProject_);
    if (!current.isNull() && workspace_.hasProject(current) && visible(workspace_.project(current)))
        return;
    const auto &projects = workspace_.projects();
    const auto first = std::ranges::find_if(projects, visible);
    const QString next = first != projects.end()             ? idText(first->id)
                         : workspace_.sharedBoards().empty() ? QString()
                                                             : sharedId();
    if (next != currentProject_) {
        currentProject_ = next;
        emit currentProjectChanged();
    }
}

void Library::refreshProjects() {
    QList<QVariantMap> rows;
    for (const ProjectEntry &project : workspace_.projects()) {
        if (project.deleted())
            continue;
        rows.append(
            {{"projectId", idText(project.id)},
             {"name", project.name()},
             {"colorIndex", project.colorIndex},
             {"boardCount", static_cast<int>(workspace_.boardsInProject(project.id).size())},
             {"admin", workspace_.projectRole(project.id, workspace_.user()) == Role::Admin},
             {"pending", !workspace_.mayAccessProject(workspace_.user(), project.id)}});
    }
    projects_->setRows(std::move(rows));
}

void Library::refreshBoards() {
    std::vector<QUuid> ids;
    const QUuid project = QUuid::fromString(currentProject_);
    if (currentProject_ == sharedId())
        ids = workspace_.sharedBoards();
    else if (workspace_.mayAccessProject(workspace_.user(), project))
        ids = workspace_.boardsInProject(project); // nothing until you are let in
    std::vector<const BoardEntry *> boards;
    for (const QUuid &id : ids)
        boards.push_back(&workspace_.board(id));
    std::ranges::sort(boards, std::greater{}, [](const BoardEntry *b) { return b->edited; });
    QList<QVariantMap> rows;
    for (const BoardEntry *board : boards) {
        if (!board->name().contains(boardFilter_.trimmed(), Qt::CaseInsensitive))
            continue;
        rows.append({{"boardId", idText(board->id)},
                     {"name", board->name()},
                     {"edited", board->edited},
                     {"admin", workspace_.boardRole(board->id, workspace_.user()) >= Role::Owner},
                     {"pending", !workspace_.mayAccessBoard(workspace_.user(), board->id)}});
    }
    boards_->setRows(std::move(rows));
}

void Library::refreshContacts() {
    QList<QVariantMap> rows;
    for (const Contact &contact : identity_.contacts())
        rows.append({{"key", QString::fromLatin1(contact.signingKey.toHex())},
                     {"name", contact.name},
                     {"fingerprint", formatFingerprint(contact.signingKey)},
                     {"verification",
                      verificationNames.at(static_cast<std::size_t>(contact.verification))}});
    contacts_->setRows(std::move(rows));
}

void Library::workspaceUpdated() {
    closeRemovedBoards();
    ensureValidSelection();
    refreshProjects();
    refreshBoards();
    emit currentProjectChanged();
    emit workspaceChanged();
}

void Library::contactsUpdated() { refreshContacts(); }

int Library::sharedBoardCount() const { return static_cast<int>(workspace_.sharedBoards().size()); }

void Library::setCurrentProjectId(const QString &id) {
    if (id == currentProject_)
        return;
    if (id != sharedId() && !attempt([&] { static_cast<void>(workspace_.project(parseId(id))); }))
        return;
    currentProject_ = id;
    refreshBoards();
    emit currentProjectChanged();
}

QString Library::currentProjectName() const {
    if (currentProject_ == sharedId())
        return tr("Shared with me");
    const QUuid id = QUuid::fromString(currentProject_);
    return workspace_.hasProject(id) ? workspace_.project(id).name() : QString();
}

bool Library::currentProjectIsAdmin() const {
    const QUuid id = QUuid::fromString(currentProject_);
    return workspace_.projectRole(id, workspace_.user()) == Role::Admin;
}

bool Library::currentProjectPending() const {
    const QUuid id = QUuid::fromString(currentProject_);
    return workspace_.hasProject(id) && !workspace_.mayAccessProject(workspace_.user(), id);
}

void Library::setBoardFilter(const QString &filter) {
    if (filter == boardFilter_)
        return;
    boardFilter_ = filter;
    refreshBoards();
    emit boardFilterChanged();
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

QString Library::userKey() const { return QString::fromLatin1(identity_.signingKey().toHex()); }

QString Library::contactCode() const { return encodeContactCard(identity_.contactCard()); }

bool Library::canAdoptIdentity() const {
    return workspace_.projects().empty() && workspace_.boards().empty();
}

void Library::copyToClipboard(const QString &text) { QGuiApplication::clipboard()->setText(text); }

QString Library::createProject(const QString &name) {
    QString id;
    if (attempt([&] { id = idText(workspace_.createProject(name, identity_.displayName())); })) {
        currentProject_ = id;
        workspaceUpdated();
    }
    return id;
}

QString Library::joinInvite(const QString &code) {
    QString id;
    attempt([&] {
        const std::optional<Invite> invite = decodeInvite(code);
        if (!invite)
            throw std::invalid_argument("That is not a PeerChalk invite");
        workspace_.join(*invite);
        id = idText(invite->id);
        currentProject_ = invite->kind == Invite::Kind::Project ? id : sharedId();
    });
    if (!id.isEmpty())
        workspaceUpdated();
    return id;
}

QString Library::projectInvite(const QString &projectId) {
    QString code;
    attempt([&] { code = encodeInvite(workspace_.inviteToProject(parseId(projectId))); });
    return code;
}

QString Library::boardInvite(const QString &boardId) {
    QString code;
    attempt([&] { code = encodeInvite(workspace_.inviteToBoard(parseId(boardId))); });
    return code;
}

void Library::renameProject(const QString &projectId, const QString &name) {
    if (attempt([&] { workspace_.renameProject(parseId(projectId), name); }))
        workspaceUpdated();
}

void Library::removeProject(const QString &projectId) {
    if (attempt([&] { workspace_.removeProject(parseId(projectId)); }))
        workspaceUpdated();
}

bool Library::isProjectAdmin(const QString &projectId) {
    return workspace_.projectRole(QUuid::fromString(projectId), workspace_.user()) == Role::Admin;
}

QString Library::createBoard(const QString &name) {
    QString id;
    const bool shared = currentProject_ == sharedId() || currentProject_.isEmpty();
    attempt([&] {
        const std::optional<QUuid> project =
            shared ? std::nullopt : std::optional<QUuid>(parseId(currentProject_));
        id = idText(workspace_.createBoard(name, project, identity_.displayName()));
    });
    if (!id.isEmpty())
        workspaceUpdated();
    return id;
}

void Library::renameBoard(const QString &boardId, const QString &name) {
    if (attempt([&] { workspace_.renameBoard(parseId(boardId), name); }))
        workspaceUpdated();
}

void Library::removeBoard(const QString &boardId) {
    if (attempt([&] { workspace_.removeBoard(parseId(boardId)); }))
        workspaceUpdated();
}

bool Library::isBoardAdmin(const QString &boardId) {
    return workspace_.boardRole(QUuid::fromString(boardId), workspace_.user()) == Role::Admin;
}

QString Library::boardName(const QString &boardId) {
    QString name;
    attempt([&] { name = workspace_.board(parseId(boardId)).name(); });
    return name;
}

QByteArray Library::boardState(const QString &boardId) {
    QByteArray state;
    attempt([&] { state = workspace_.boardState(parseId(boardId)); });
    return state;
}

void Library::saveBoardState(const QString &boardId, const QByteArray &state) {
    if (attempt([&] { workspace_.saveBoardState(parseId(boardId), state); }))
        refreshBoards();
}

} // namespace peerchalk
