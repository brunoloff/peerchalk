// Workspace: roles, boards in projects, removal, invites and joining.
#include "library/files.hpp"
#include "library/workspace.hpp"
#include "library/workspace_support.hpp"

#include <algorithm>
#include <stdexcept>

namespace peerchalk {

using namespace workspace_detail;
using Action = AccessOp::Action;

namespace {

// Checks what replay would accept, so mistakes get a clear message instead of a change
// that silently has no effect.
void requireMayChangeRole(const AccessState &state, const QByteArray &signer,
                          const QByteArray &user, Role role) {
    const Role mine = state.role(signer);
    if (user == state.admin)
        throw std::invalid_argument("The admin's role changes only by handing it to someone else");
    const bool membersOnly = state.role(user) <= Role::Member && role <= Role::Member;
    if (mine == Role::Admin || (mine == Role::Owner && membersOnly))
        return;
    throw std::invalid_argument(mine == Role::Owner ? "Only the admin can change who is an owner"
                                                    : "Only owners can change who has access");
}

} // namespace

void Workspace::setProjectRole(const QUuid &id, const QByteArray &user, const QString &name,
                               Role role) {
    ProjectEntry &entry = findProject(id);
    requireMayChangeRole(entry.state, user_, user, role);
    AccessOp op = change(ResourceKind::Project, id, Action::SetRole);
    op.user = user;
    op.name = name.trimmed();
    op.role = role;
    record(entry.ops, entry.state.clock, op);
}

void Workspace::setBoardRole(const QUuid &id, const QByteArray &user, const QString &name,
                             Role role) {
    BoardEntry &entry = findBoard(id);
    requireMayChangeRole(entry.state, user_, user, role);
    AccessOp op = change(ResourceKind::Board, id, Action::SetRole);
    op.user = user;
    op.name = name.trimmed();
    op.role = role;
    record(entry.ops, entry.state.clock, op);
}

void Workspace::transferProjectAdmin(const QUuid &id, const QByteArray &user, const QString &name) {
    ProjectEntry &entry = findProject(id);
    if (entry.state.role(user_) != Role::Admin || user == user_)
        throw std::invalid_argument("Only the admin can hand the admin role to someone else");
    AccessOp op = change(ResourceKind::Project, id, Action::TransferAdmin);
    op.user = user;
    op.name = name.trimmed();
    record(entry.ops, entry.state.clock, op);
}

void Workspace::transferBoardAdmin(const QUuid &id, const QByteArray &user, const QString &name) {
    BoardEntry &entry = findBoard(id);
    if (entry.state.role(user_) != Role::Admin || user == user_)
        throw std::invalid_argument("Only the admin can hand the admin role to someone else");
    AccessOp op = change(ResourceKind::Board, id, Action::TransferAdmin);
    op.user = user;
    op.name = name.trimmed();
    record(entry.ops, entry.state.clock, op);
}

void Workspace::setBoardInProject(const QUuid &boardId, const QUuid &projectId, bool included) {
    const BoardEntry &board = this->board(boardId);
    ProjectEntry &project = findProject(projectId);
    const bool ownsBoth =
        project.state.role(user_) >= Role::Owner && board.state.role(user_) >= Role::Owner;
    const bool adminRemoving = project.state.role(user_) == Role::Admin && !included;
    if (!ownsBoth && !adminRemoving)
        throw std::invalid_argument(
            "Only someone who owns both the project and the board can do that");
    if (board.creator.isEmpty())
        throw std::invalid_argument("This board's origin is unknown here");
    AccessOp op = change(ResourceKind::Project, projectId, Action::SetBoard);
    op.board = boardId;
    op.user = board.creator;
    op.included = included;
    record(project.ops, project.state.clock, op);
}

void Workspace::removeProject(const QUuid &id) {
    ProjectEntry &entry = findProject(id);
    if (entry.state.role(user_) == Role::Admin) {
        record(entry.ops, entry.state.clock, change(ResourceKind::Project, id, Action::Delete));
        return;
    }
    projects_.erase(std::ranges::find(projects_, id, &ProjectEntry::id));
    recompute();
    save();
}

void Workspace::removeBoard(const QUuid &id) {
    BoardEntry &entry = findBoard(id);
    removeFile(boardPath(id));
    if (entry.state.role(user_) == Role::Admin) {
        record(entry.ops, entry.state.clock, change(ResourceKind::Board, id, Action::Delete));
        return;
    }
    boards_.erase(std::ranges::find(boards_, id, &BoardEntry::id));
    recompute();
    save();
}

Invite Workspace::inviteToProject(const QUuid &id) const {
    const ProjectEntry &entry = project(id);
    if (entry.state.role(user_) < Role::Owner)
        throw std::invalid_argument("Only the project's owners can invite people");
    return {Invite::Kind::Project, id, entry.name(), entry.capability, entry.creator};
}

Invite Workspace::inviteToBoard(const QUuid &id) const {
    const BoardEntry &entry = board(id);
    if (entry.state.role(user_) < Role::Owner)
        throw std::invalid_argument("Only the board's owners can invite people");
    if (entry.capability.isEmpty())
        throw std::invalid_argument(
            "This board can only be shared through its project from this device");
    return {Invite::Kind::Board, id, entry.name(), entry.capability, entry.creator};
}

void Workspace::join(const Invite &invite) {
    if (invite.capability.size() != capabilitySize || invite.creator.size() != signingKeySize)
        throw std::invalid_argument("The invite is damaged");
    if (invite.kind == Invite::Kind::Project) {
        if (hasProject(invite.id))
            throw std::invalid_argument("You are already in this project");
        projects_.push_back({invite.id,
                             invite.capability,
                             invite.creator,
                             requireName(invite.name),
                             static_cast<int>(projects_.size()) % projectColorCount,
                             {},
                             {}});
    } else if (hasBoard(invite.id)) {
        BoardEntry &entry = findBoard(invite.id);
        if (!entry.capability.isEmpty())
            throw std::invalid_argument("You already have this board");
        entry.capability = invite.capability; // known through a project; now invited directly
    } else {
        boards_.push_back({invite.id,
                           invite.capability,
                           invite.creator,
                           requireName(invite.name),
                           QDateTime::currentDateTimeUtc(),
                           {},
                           {}});
    }
    recompute();
    save();
}

} // namespace peerchalk
