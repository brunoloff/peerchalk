#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace peerchalk {

// Small file helpers for the library stores. Failures throw std::runtime_error that
// names the file and the reason.

// The file's contents, or nullopt when it does not exist.
[[nodiscard]] std::optional<QByteArray> readFile(const QString &path);
// Replaces the file atomically. With ownerOnly the file is readable only by its owner.
void writeFile(const QString &path, const QByteArray &contents, bool ownerOnly);
void removeFile(const QString &path);

// A JSON object file with a "version" field equal to version, or nullopt when the
// file does not exist. Throws std::runtime_error for anything else.
[[nodiscard]] std::optional<QJsonObject> readJsonObject(const QString &path, int version);
void writeJsonObject(const QString &path, QJsonObject object, int version, bool ownerOnly);

// A base64 field decoded, required to have exactly size bytes; throws std::runtime_error
// naming the field otherwise.
[[nodiscard]] QByteArray base64Field(const QJsonObject &object, const QString &key, qsizetype size);

} // namespace peerchalk
