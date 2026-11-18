#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <map>
#include <stdexcept>

namespace peerchalk {

// What a backup holds. Device keys are never included: each installation has its own, and
// two sharing one would mistake each other for themselves.
enum class BackupKind {
    Everything, // identity, profile, known people, projects and boards
    Identity,   // identity, profile and known people
    People,     // known people only
};

inline constexpr qsizetype minBackupPassphraseLength = 12;

struct BackupContents {
    BackupKind kind{BackupKind::Everything};
    QDateTime created;
    std::map<QString, QByteArray> files; // relative path → contents
};

// The passphrase is wrong or the file was altered; the two cannot be told apart.
class BackupAccessError : public std::invalid_argument {
  public:
    BackupAccessError();
};

// An encrypted backup of an account folder: Argon2id derives the key from the passphrase
// and XChaCha20-Poly1305 protects the contents, with the header authenticated. Throws
// std::invalid_argument for a short passphrase and std::runtime_error for unreadable data.
[[nodiscard]] QByteArray createBackup(const QString &accountDirectory, BackupKind kind,
                                      const QString &passphrase);
// Throws BackupAccessError, or std::invalid_argument when it is not a usable backup.
[[nodiscard]] BackupContents openBackup(const QByteArray &file, const QString &passphrase);
void writeBackupFiles(const BackupContents &backup, const QString &directory);

} // namespace peerchalk
