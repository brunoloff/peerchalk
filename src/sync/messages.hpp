#pragma once

#include <utility>

#include <QByteArray>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QUuid>

#include <optional>
#include <variant>
#include <vector>

namespace peerchalk::sync {

// PeerChalk's messages between two devices over a p2pdc connection: one type byte, then
// a payload (compact JSON, or a board id and a Yrs v1 update). Everything received is
// untrusted: decoding returns nullopt for anything malformed, and callers still check
// signatures and access.

// First message on every connection: who the device is and acts for.
struct Hello {
    QByteArray certificatePayload;
    QByteArray certificateSignature;
    QString displayName;
    int colorIndex{0};
};

// Signed access changes (payload, signature) the receiver checks and may store (see
// Workspace::acceptOps). Anyone who has them passes them on.
struct AccessChanges {
    std::vector<std::pair<QByteArray, QByteArray>> ops;
};

// Between one user's devices only: the workspace (with capabilities) and contacts.
struct Personal {
    QJsonObject workspace;
    QJsonObject contacts;
};

// Changes to a board's Yrs document.
struct BoardUpdate {
    QUuid board;
    QByteArray update;
};

// The sender's new display name and colour, after a change.
struct Profile {
    QString displayName;
    int colorIndex{0};
};

// Which board the sender has open, and where its pointer is (world coordinates).
struct Presence {
    QUuid board; // null: no board open
    QPointF cursor;
    bool cursorVisible{false};
};

using Message = std::variant<Hello, AccessChanges, Personal, BoardUpdate, Presence, Profile>;

[[nodiscard]] QByteArray encode(const Message &message);
[[nodiscard]] std::optional<Message> decode(const QByteArray &bytes);

} // namespace peerchalk::sync
