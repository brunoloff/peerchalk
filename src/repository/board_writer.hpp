#pragma once

#include "board/board.hpp"
#include "library/user_identity.hpp"
#include "repository/board_loader.hpp"

#include <QByteArray>
#include <QString>
#include <QUuid>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace peerchalk::repository {

// Writing a repository board's files. Each function returns the files to write, by path
// relative to boards/<id>/, with nullopt for files to delete; committing them is the caller's.
using FileChanges = std::map<QString, std::optional<QByteArray>>;

// Who writes: a user and one of their devices.
struct Author {
    QByteArray userKey;
    DeviceCertificate certificate;
    Signer user;   // signs with the user key
    Signer device; // signs with the device key
};
[[nodiscard]] Author authorOf(const UserIdentity &identity);

struct NewBoard {
    QUuid id;
    QByteArray capability; // the network secret of epoch 0
    FileChanges files;
};
// A board you administer: board.json, its Create entry, the secret sealed to you, and your
// change file holding its name. Throws std::invalid_argument for an empty name.
[[nodiscard]] NewBoard createBoard(const QString &name, const QString &displayName,
                                   const Author &author);

[[nodiscard]] QString changeFilePath(const QByteArray &device);
// The author's change file: everything the document holds beyond the loaded snapshots.
[[nodiscard]] QByteArray changeFile(const Board &document, const LoadedBoard &loaded,
                                    const Author &author,
                                    const std::vector<std::uint64_t> &clients);
// Whether the accepted snapshots already hold everything in the device's change file, so
// that the device may delete it.
[[nodiscard]] bool changesCovered(const LoadedBoard &loaded, const QByteArray &device);

// A snapshot of the whole document that replaces the loaded snapshots and the author's own
// change file. Concurrent compactions merge: both snapshots stay, and both are valid.
struct Compaction {
    QByteArray hash; // of the new snapshot
    FileChanges files;
};
[[nodiscard]] Compaction compact(const Board &document, const LoadedBoard &loaded,
                                 const Author &author, const std::vector<std::uint64_t> &clients);

// Gives a user a role (Member or Owner) and seals the latest secret to them. Throws
// std::invalid_argument when the author may not, or for Role::None (see removeMember).
[[nodiscard]] FileChanges setRole(const LoadedBoard &loaded, const Author &author,
                                  const QByteArray &capability, const QByteArray &user,
                                  const QString &name, Role role);
// Removes a user: a snapshot of what the author knows, the removal naming it, and a new
// epoch of the network secret (newCapability) sealed to everyone who remains.
[[nodiscard]] FileChanges removeMember(const Board &document, const LoadedBoard &loaded,
                                       const Author &author, const QByteArray &user,
                                       const std::vector<std::uint64_t> &clients,
                                       const QByteArray &newCapability);

// The latest network secret sealed to this identity's user, or nullopt when there is none.
[[nodiscard]] std::optional<QByteArray> openSecret(const LoadedBoard &loaded,
                                                   const UserIdentity &identity);

} // namespace peerchalk::repository
