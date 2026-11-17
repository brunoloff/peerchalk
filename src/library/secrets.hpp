#pragma once

#include <p2pdc/discovery_key.h>
#include <p2pdc/security.h>

#include <QByteArray>

#include <span>
#include <stdexcept>
#include <variant>

namespace peerchalk {

// A buffer for secret material that is overwritten when it goes out of scope, including
// when an exception unwinds.
struct SecretBytes {
    QByteArray bytes;
    explicit SecretBytes(QByteArray value) : bytes(std::move(value)) {}
    SecretBytes(const SecretBytes &) = delete;
    SecretBytes &operator=(const SecretBytes &) = delete;
    ~SecretBytes() { bytes.fill('\0'); }

    [[nodiscard]] std::span<std::byte> span() {
        return {reinterpret_cast<std::byte *>(bytes.data()),
                static_cast<std::size_t>(bytes.size())};
    }
    [[nodiscard]] p2pdc::ByteView view() const {
        return {reinterpret_cast<const std::byte *>(bytes.constData()),
                static_cast<std::size_t>(bytes.size())};
    }
};

// The value of a p2pdc result, or std::runtime_error with `what`.
template <class T> T unwrap(p2pdc::CryptoResult<T> result, const char *what) {
    if (auto *value = std::get_if<T>(&result))
        return std::move(*value);
    throw std::runtime_error(what);
}

[[nodiscard]] inline QByteArray toBytes(std::span<const std::byte> bytes) {
    return {reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size())};
}

[[nodiscard]] inline p2pdc::ByteView byteView(const QByteArray &bytes) {
    return {reinterpret_cast<const std::byte *>(bytes.constData()),
            static_cast<std::size_t>(bytes.size())};
}

// A fresh 32-byte discovery capability for a project or board.
[[nodiscard]] inline QByteArray newCapability() {
    return toBytes(
        unwrap(p2pdc::generate_discovery_capability(), "Could not generate a discovery secret"));
}

} // namespace peerchalk
