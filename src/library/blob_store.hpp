#pragma once

#include "board/stroke_codec.hpp"

#include <QByteArray>
#include <QString>

#include <map>
#include <memory>
#include <mutex>

namespace peerchalk {

// Files kept by the SHA-256 of their contents, such as the PDFs that page boards show: each
// is <directory>/<hash in lowercase hex>, written once and never changed. Safe to use from
// several threads.
class BlobStore {
  public:
    explicit BlobStore(QString directory);

    [[nodiscard]] static QString hashOf(const QByteArray &data);
    [[nodiscard]] static bool isHash(const QString &text);

    // Stores data unless it is already there, and returns its hash. Throws
    // std::runtime_error when it cannot be written.
    QString put(const QByteArray &data);
    [[nodiscard]] bool has(const QString &hash) const;
    // The blob's contents, shared by everyone holding them; null when it is missing, not a
    // hash, or no longer matches its hash.
    [[nodiscard]] std::shared_ptr<const Bytes> get(const QString &hash) const;

  private:
    [[nodiscard]] QString pathOf(const QString &hash) const;

    QString directory_;
    mutable std::mutex lock_;
    mutable std::map<QString, std::weak_ptr<const Bytes>> loaded_;
};

} // namespace peerchalk
