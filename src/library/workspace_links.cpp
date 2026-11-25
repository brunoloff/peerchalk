// Workspace: links that let people join a board without being approved one by one.
#include "library/secrets.hpp"
#include "library/workspace.hpp"
#include "library/workspace_support.hpp"

#include <QCryptographicHash>

#include <algorithm>
#include <stdexcept>

namespace peerchalk {

using namespace workspace_detail;
using Action = AccessOp::Action;

namespace {

// The signed log is readable by anyone who can find the board, so it holds only this.
QByteArray linkHash(const QByteArray &token) {
    return QCryptographicHash::hash(QByteArrayLiteral("peerchalk/board-link/v1") + token,
                                    QCryptographicHash::Sha256);
}

} // namespace

bool Workspace::isBoardOwner(const QByteArray &user, const QUuid &boardId) const {
    if (boardRole(boardId, user) >= Role::Owner)
        return true;
    return std::ranges::any_of(projects_, [&](const ProjectEntry &project) {
        return projectRole(project.id, user) >= Role::Owner &&
               std::ranges::find(project.state.boards, boardId) != project.state.boards.end();
    });
}

bool Workspace::isRepositoryBoard(const QUuid &boardId) const {
    return hasBoard(boardId) && board(boardId).repository;
}

void Workspace::markRepositoryBoard(const QUuid &boardId) {
    findBoard(boardId).repository = true;
    save();
}

void Workspace::recordBoardLink(const QUuid &id, const QByteArray &hash) {
    BoardEntry &entry = findBoard(id);
    if (entry.state.role(user_) < Role::Owner)
        throw std::invalid_argument("Only the board's owners can manage its link");
    AccessOp op = change(ResourceKind::Board, id, Action::SetLink);
    op.linkHash = hash;
    record(entry.ops, entry.state.clock, op);
}

BoardLink Workspace::createBoardLink(const QUuid &id) {
    const Invite invite = inviteToBoard(id); // checks the role and the discovery secret
    const QByteArray token = newCapability();
    recordBoardLink(id, linkHash(token));
    findBoard(id).linkToken = token;
    save();
    return {id, invite.name, invite.capability, invite.creator, token};
}

std::optional<BoardLink> Workspace::currentBoardLink(const QUuid &id) const {
    const BoardEntry &entry = board(id);
    if (entry.state.role(user_) < Role::Owner || entry.capability.isEmpty() ||
        entry.linkToken.isEmpty() || linkHash(entry.linkToken) != entry.state.linkHash)
        return std::nullopt;
    return BoardLink{id, entry.name(), entry.capability, entry.creator, entry.linkToken};
}

bool Workspace::boardHasLink(const QUuid &id) const {
    return hasBoard(id) && !board(id).state.linkHash.isEmpty();
}

void Workspace::disableBoardLink(const QUuid &id) {
    recordBoardLink(id, {});
    findBoard(id).linkToken.clear();
    save();
}

void Workspace::joinByLink(const BoardLink &link) {
    if (link.capability.size() != capabilitySize || link.creator.size() != signingKeySize ||
        link.token.size() != linkTokenSize)
        throw std::invalid_argument("The link is damaged");
    if (hasBoard(link.id)) {
        BoardEntry &entry = findBoard(link.id);
        if (!entry.creator.isEmpty() && entry.creator != link.creator)
            throw std::invalid_argument("This link does not match the board you have");
        entry.creator = link.creator;
        entry.capability = link.capability;
        if (entry.state.role(user_) < Role::Owner)
            entry.linkToken = link.token;
    } else {
        boards_.push_back({link.id,
                           link.capability,
                           link.creator,
                           requireName(link.name),
                           QDateTime::currentDateTimeUtc(),
                           {},
                           {},
                           link.token});
    }
    recompute();
    save();
}

bool Workspace::admitByLink(const QUuid &id, const QByteArray &user, const QString &name,
                            const QByteArray &token) {
    if (!hasBoard(id))
        return false;
    const AccessState &state = board(id).state;
    if (state.role(user_) < Role::Owner || state.linkHash.isEmpty() ||
        linkHash(token) != state.linkHash || state.role(user) != Role::None ||
        state.removed.contains(user))
        return false;
    setBoardRole(id, user, name, Role::Member);
    return true;
}

} // namespace peerchalk
