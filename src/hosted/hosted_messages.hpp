#pragma once

#include "repository/board_loader.hpp"

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QUuid>

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace peerchalk::hosted {

// The messages between a dashboard and the PeerChalk page it hosts (docs/board-files.md,
// "Hosted mode"). The page's bridge (web_hosted.js) turns each Uint8Array into
// {"$bytes": base64} and back, so C++ sees plain JSON.

// A message the session cannot use: it is answered with an error, not acted on.
class MessageError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] QByteArray bytesOf(const QJsonValue &value, const char *what);
[[nodiscard]] QJsonValue bytesValue(const QByteArray &bytes);

// "open": the board and who this device is.
struct OpenMessage {
    QUuid board;
    repository::BoardFiles files;   // by path under boards/<id>/
    std::vector<QByteArray> access; // entries from the branch's history and seen before
    QByteArray secret;              // the board's network secret
    QString transferCode;           // the user identity, as PeerChalk's "Link a device" code
    QByteArray deviceSeed;
    QByteArray recipientSecret;
    QString deviceName;
    QString displayName;
    int colorIndex{0};
    bool canPush{false};
    std::vector<std::uint64_t> clients; // Yrs client ids this device wrote the board with
    // host.backLabel: where the board's Back button leads, such as "Boards"; empty: no button.
    QString backLabel;
};
[[nodiscard]] OpenMessage parseOpen(const QJsonObject &message);

// "update": files that changed on the branch (null: deleted) and access entries.
struct UpdateMessage {
    std::map<QString, std::optional<QByteArray>> files;
    std::vector<QByteArray> access;
    std::optional<bool> canPush;
};
[[nodiscard]] UpdateMessage parseUpdate(const QJsonObject &message);

} // namespace peerchalk::hosted
