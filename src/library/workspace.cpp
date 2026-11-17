#include "library/workspace.hpp"

#include "library/files.hpp"
#include "library/secrets.hpp"
#include "library/workspace_support.hpp"

#include <QDir>

#include <algorithm>
#include <stdexcept>

namespace peerchalk {

using namespace workspace_detail;
using Action = AccessOp::Action;

Workspace::Workspace(QString directory, QByteArray user, Signer signer)
    : directory_(std::move(directory)), user_(std::move(user)), signer_(std::move(signer)) {
    load();
}

void Workspace::resetUser(QByteArray user) {
    if (!projects_.empty() || !boards_.empty())
        throw std::invalid_argument("This device already has projects or boards");
    user_ = std::move(user);
}

const ProjectEntry &Workspace::project(const QUuid &id) const {
    const auto it = std::ranges::find(projects_, id, &ProjectEntry::id);
    if (it == projects_.end())
        throw std::invalid_argument("Unknown project");
    return *it;
}

const BoardEntry &Workspace::board(const QUuid &id) const {
    const auto it = std::ranges::find(boards_, id, &BoardEntry::id);
    if (it == boards_.end())
        throw std::invalid_argument("Unknown board");
    return *it;
}

bool Workspace::hasProject(const QUuid &id) const {
    return std::ranges::find(projects_, id, &ProjectEntry::id) != projects_.end();
}

bool Workspace::hasBoard(const QUuid &id) const {
    return std::ranges::find(boards_, id, &BoardEntry::id) != boards_.end();
}

ProjectEntry &Workspace::findProject(const QUuid &id) {
    return const_cast<ProjectEntry &>(std::as_const(*this).project(id));
}

BoardEntry &Workspace::findBoard(const QUuid &id) {
    return const_cast<BoardEntry &>(std::as_const(*this).board(id));
}

Role Workspace::projectRole(const QUuid &id, const QByteArray &user) const {
    if (!hasProject(id) || project(id).deleted())
        return Role::None;
    return project(id).state.role(user);
}

Role Workspace::boardRole(const QUuid &id, const QByteArray &user) const {
    if (!hasBoard(id) || board(id).deleted())
        return Role::None;
    return board(id).state.role(user);
}

bool Workspace::mayAccessProject(const QByteArray &user, const QUuid &projectId) const {
    return projectRole(projectId, user) >= Role::Member;
}

bool Workspace::mayAccessBoard(const QByteArray &user, const QUuid &boardId) const {
    if (!hasBoard(boardId) || !board(boardId).state.exists || board(boardId).deleted())
        return false;
    if (boardRole(boardId, user) >= Role::Member)
        return true;
    return std::ranges::any_of(projects_, [&](const ProjectEntry &project) {
        return mayAccessProject(user, project.id) &&
               std::ranges::find(project.state.boards, boardId) != project.state.boards.end();
    });
}

std::vector<QUuid> Workspace::boardsInProject(const QUuid &projectId) const {
    std::vector<QUuid> result;
    if (!hasProject(projectId) || project(projectId).deleted())
        return result;
    for (const QUuid &id : project(projectId).state.boards)
        if (hasBoard(id) && board(id).state.exists && !board(id).deleted())
            result.push_back(id);
    return result;
}

std::vector<QUuid> Workspace::sharedBoards() const {
    std::vector<QUuid> inProjects;
    for (const ProjectEntry &project : projects_)
        if (mayAccessProject(user_, project.id))
            std::ranges::copy(boardsInProject(project.id), std::back_inserter(inProjects));
    std::vector<QUuid> result;
    for (const BoardEntry &board : boards_) {
        const bool shown = std::ranges::find(inProjects, board.id) != inProjects.end();
        // Boards joined by invite stay listed while their owners have yet to let you in.
        const bool joined = !board.capability.isEmpty();
        if (!shown && !board.deleted() && (joined || boardRole(board.id, user_) >= Role::Member))
            result.push_back(board.id);
    }
    return result;
}

void Workspace::record(std::vector<SignedOp> &ops, quint64 clock, AccessOp op) {
    op.clock = clock + 1;
    op.signer = user_;
    ops.push_back(signOp(op, signer_));
    recompute();
    save();
}

// Every project's and board's state, replayed from its changes. Boards come first:
// a project's SetBoard changes are checked against board roles.
void Workspace::recompute() {
    for (BoardEntry &board : boards_)
        board.state = replay(board.ops, ResourceKind::Board, board.id, board.creator, {});
    const BoardRoleLookup boardRoleLookup = [this](const QUuid &id, const QByteArray &creator,
                                                   const QByteArray &user) {
        if (!hasBoard(id) || board(id).creator != creator)
            return Role::None;
        return board(id).state.role(user);
    };
    for (ProjectEntry &project : projects_)
        project.state = replay(project.ops, ResourceKind::Project, project.id, project.creator,
                               boardRoleLookup);
}

QUuid Workspace::createProject(const QString &name, const QString &displayName) {
    const QUuid id = QUuid::createUuid();
    projects_.push_back({id,
                         newCapability(),
                         user_,
                         requireName(name),
                         static_cast<int>(projects_.size()) % projectColorCount,
                         {},
                         {}});
    AccessOp op = change(ResourceKind::Project, id, Action::Create);
    op.name = projects_.back().inviteName;
    op.signerName = displayName;
    record(projects_.back().ops, 0, op);
    return id;
}

QUuid Workspace::createBoard(const QString &name, const std::optional<QUuid> &projectId,
                             const QString &displayName) {
    if (projectId && projectRole(*projectId, user_) < Role::Owner)
        throw std::invalid_argument("Only the project's owners can add boards to it");
    const QUuid id = QUuid::createUuid();
    boards_.push_back(
        {id, newCapability(), user_, requireName(name), QDateTime::currentDateTimeUtc(), {}, {}});
    AccessOp op = change(ResourceKind::Board, id, Action::Create);
    op.name = boards_.back().inviteName;
    op.signerName = displayName;
    record(boards_.back().ops, 0, op);
    if (projectId)
        setBoardInProject(id, *projectId, true);
    return id;
}

void Workspace::renameProject(const QUuid &id, const QString &name) {
    ProjectEntry &entry = findProject(id);
    if (entry.state.role(user_) != Role::Admin)
        throw std::invalid_argument("Only the project's admin can rename it");
    AccessOp op = change(ResourceKind::Project, id, Action::Rename);
    op.name = requireName(name);
    record(entry.ops, entry.state.clock, op);
}

void Workspace::renameBoard(const QUuid &id, const QString &name) {
    BoardEntry &entry = findBoard(id);
    if (entry.state.role(user_) != Role::Admin)
        throw std::invalid_argument("Only the board's admin can rename it");
    AccessOp op = change(ResourceKind::Board, id, Action::Rename);
    op.name = requireName(name);
    record(entry.ops, entry.state.clock, op);
}

QString Workspace::boardPath(const QUuid &id) const {
    return QDir(directory_)
        .filePath(QStringLiteral("boards/%1.yrs").arg(id.toString(QUuid::WithoutBraces)));
}

QByteArray Workspace::boardState(const QUuid &id) const {
    static_cast<void>(board(id)); // throws for unknown boards
    return readFile(boardPath(id)).value_or(QByteArray());
}

void Workspace::saveBoardState(const QUuid &id, const QByteArray &state) {
    BoardEntry &entry = findBoard(id);
    writeFile(boardPath(id), state, true);
    entry.edited = QDateTime::currentDateTimeUtc();
    save();
}

} // namespace peerchalk
