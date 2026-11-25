#pragma once

#include "board/stroke_codec.hpp"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <optional>
#include <stdexcept>

namespace peerchalk::repository {

// Files on the repository branch are untrusted: anything malformed throws FileError with a
// reason fit to show the user, and the reader skips that file.
class FileError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Keys and hashes appear in paths and files as lowercase hex.
[[nodiscard]] QString toHex(const QByteArray &bytes);
// Exactly size bytes of lowercase hex, or nullopt.
[[nodiscard]] std::optional<QByteArray> fromHex(const QString &text, qsizetype size);
[[nodiscard]] QByteArray sha256(const QByteArray &bytes);

[[nodiscard]] QByteArray toByteArray(ByteView bytes);
[[nodiscard]] ByteView byteView(const QByteArray &bytes);

// A compact JSON object; throws FileError when the bytes are not one.
[[nodiscard]] QJsonObject parseObject(const QByteArray &bytes, const char *what);
// A hex field of exactly size bytes; throws FileError naming the field otherwise.
[[nodiscard]] QByteArray hexField(const QJsonObject &object, const char *key, qsizetype size);
// A base64 field; throws FileError naming the field when it is not base64.
[[nodiscard]] QByteArray base64Field(const QJsonObject &object, const char *key);
[[nodiscard]] QString base64(const QByteArray &bytes);

} // namespace peerchalk::repository
