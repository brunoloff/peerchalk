#pragma once

#include "library/signatures.hpp"

#include <QByteArray>
#include <QString>
#include <QUuid>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace peerchalk {

// Roles on a project or board; each includes the ones below it.
enum class Role { None = 0, Member = 1, Owner = 2, Admin = 3 };

enum class ResourceKind { Project, Board };

// One signed change to who may do what with a project or board.
struct AccessOp {
    enum class Action { Create, Rename, SetRole, TransferAdmin, SetBoard, Delete, SetLink };

    ResourceKind kind{ResourceKind::Project};
    QUuid resource;
    quint64 clock{0}; // Lamport clock: orders changes the same way on every device
    QByteArray signer;
    Action action{Action::Create};
    QString name;          // Create (resource name), Rename, SetRole/TransferAdmin (user's name)
    QString signerName;    // Create: the creator's display name
    QByteArray user;       // SetRole, TransferAdmin; SetBoard: the board's creator
    Role role{Role::None}; // SetRole: Member, Owner, or None to remove
    QUuid board;           // SetBoard (projects only)
    bool included{false};  // SetBoard
    QByteArray linkHash;   // SetLink (boards only): the valid link's hash; empty turns it off
};

// A change together with the exact bytes its signer signed.
struct SignedOp {
    AccessOp op;
    QByteArray payload;
    QByteArray signature;
    QByteArray hash; // SHA-256 of the payload: identifies the change
};

// Signs a change; op.signer must be the signer's user key.
[[nodiscard]] SignedOp signOp(const AccessOp &op, const Signer &signer);
// Parses and checks the signature of a received change; nullopt if anything is wrong.
[[nodiscard]] std::optional<SignedOp> verifyOp(const QByteArray &payload,
                                               const QByteArray &signature);

// Who may do what, as the result of replaying a resource's changes.
struct AccessState {
    bool exists{false}; // a valid Create was found
    QString name;
    QByteArray admin;
    std::map<QByteArray, Role> roles; // includes the admin
    std::map<QByteArray, QString> names;
    std::vector<QUuid> boards; // projects: boards in the project
    // Projects: each board's creator, recorded by whoever added it; anchors the board's
    // own changes for members who learn of it through the project.
    std::map<QUuid, QByteArray> boardCreators;
    bool deleted{false};
    QByteArray linkHash;          // boards: hash of the link that admits people, if any
    std::set<QByteArray> removed; // people taken off and not added back: links do not readmit
    quint64 clock{0};             // highest clock seen, valid or not

    [[nodiscard]] Role role(const QByteArray &user) const;
};

// The role a user has on the board with the given creator; used to check a project's
// SetBoard changes. None when the board or that creator is unknown.
using BoardRoleLookup =
    std::function<Role(const QUuid &board, const QByteArray &creator, const QByteArray &user)>;

// Replays changes in (clock, signer, hash) order. Only a Create signed by `creator` (the
// key an invite names, or a project recorded for the board) counts. Each later change
// applies only if its signer had the right at that point:
// - the admin may add, remove, promote and demote anyone but themself, hand the admin
//   role to someone (staying an owner), rename, and delete;
// - an owner may add and remove members (not owners) and set or turn off a board's link;
// - on projects, someone who owns both the project and a board may add or remove that
//   board; the project admin may also remove any board.
[[nodiscard]] AccessState replay(const std::vector<SignedOp> &ops, ResourceKind kind,
                                 const QUuid &resource, const QByteArray &creator,
                                 const BoardRoleLookup &boardRole);

} // namespace peerchalk
