// Replaying access changes: who may make which change.
#include "library/access_log.hpp"

#include <algorithm>

namespace peerchalk {
namespace {

using Action = AccessOp::Action;

bool isCreateFor(const SignedOp &signedOp, ResourceKind kind, const QUuid &resource,
                 const QByteArray &creator) {
    const AccessOp &op = signedOp.op;
    return op.action == Action::Create && op.kind == kind && op.resource == resource &&
           op.signer == creator;
}

bool applyRole(AccessState &state, const AccessOp &op, Role signerRole) {
    const Role current = state.role(op.user);
    if (op.user == state.admin)
        return false; // the admin hands over the role instead of losing it
    const bool membersOnly = current <= Role::Member && op.role <= Role::Member;
    if (signerRole < Role::Admin && !(signerRole == Role::Owner && membersOnly))
        return false;
    if (op.role == Role::None) {
        state.roles.erase(op.user);
        state.removed.insert(op.user);
        return true;
    }
    state.removed.erase(op.user);
    state.roles[op.user] = op.role;
    if (!op.name.isEmpty())
        state.names[op.user] = op.name;
    return true;
}

bool applyBoard(AccessState &state, const AccessOp &op, Role signerRole,
                const BoardRoleLookup &boardRole) {
    const bool ownsBoth =
        signerRole >= Role::Owner && boardRole(op.board, op.user, op.signer) >= Role::Owner;
    const bool adminRemoving = signerRole == Role::Admin && !op.included;
    if (!ownsBoth && !adminRemoving)
        return false;
    std::erase(state.boards, op.board);
    state.boardCreators.erase(op.board);
    if (op.included) {
        state.boards.push_back(op.board);
        state.boardCreators[op.board] = op.user;
    }
    return true;
}

// Applies one change if its signer may make it; returns whether it applied.
bool apply(AccessState &state, const AccessOp &op, const BoardRoleLookup &boardRole) {
    const Role signerRole = state.role(op.signer);
    switch (op.action) {
    case Action::Create:
        return false; // only the first counts
    case Action::Rename:
        if (signerRole != Role::Admin)
            return false;
        state.name = op.name;
        return true;
    case Action::SetRole:
        return applyRole(state, op, signerRole);
    case Action::TransferAdmin:
        if (signerRole != Role::Admin || op.user == state.admin)
            return false;
        state.roles[state.admin] = Role::Owner;
        state.admin = op.user;
        state.roles[op.user] = Role::Admin;
        if (!op.name.isEmpty())
            state.names[op.user] = op.name;
        return true;
    case Action::SetBoard:
        return applyBoard(state, op, signerRole, boardRole);
    case Action::SetLink:
        if (signerRole < Role::Owner)
            return false;
        state.linkHash = op.linkHash;
        return true;
    case Action::Delete:
        if (signerRole != Role::Admin)
            return false;
        state.deleted = true;
        return true;
    }
    return false;
}

} // namespace

AccessState replay(const std::vector<SignedOp> &ops, ResourceKind kind, const QUuid &resource,
                   const QByteArray &creator, const BoardRoleLookup &boardRole) {
    std::vector<const SignedOp *> ordered;
    for (const SignedOp &op : ops)
        if (op.op.kind == kind && op.op.resource == resource)
            ordered.push_back(&op);
    std::ranges::sort(ordered, [](const SignedOp *a, const SignedOp *b) {
        return std::tie(a->op.clock, a->op.signer, a->hash) <
               std::tie(b->op.clock, b->op.signer, b->hash);
    });
    AccessState state;
    for (const SignedOp *op : ordered)
        state.clock = std::max(state.clock, op->op.clock);
    const auto create = std::ranges::find_if(
        ordered, [&](const SignedOp *op) { return isCreateFor(*op, kind, resource, creator); });
    if (create == ordered.end())
        return state;
    state.exists = true;
    state.name = (*create)->op.name;
    state.admin = (*create)->op.signer;
    state.roles[state.admin] = Role::Admin;
    state.names[state.admin] = (*create)->op.signerName;
    for (auto it = std::next(create); it != ordered.end() && !state.deleted; ++it)
        apply(state, (*it)->op, boardRole);
    return state;
}

} // namespace peerchalk
