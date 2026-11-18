#pragma once

#include "board/stroke.hpp"

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

// This version of the protocol, and the oldest one it can still talk to. Raise the first
// for additions older peers can ignore, and the second only for incompatible changes.
inline constexpr int protocolVersion = 3;
inline constexpr int minSupportedProtocol = 3;

// First message on every connection: who the device is and acts for.
struct Hello {
    QByteArray certificatePayload;
    QByteArray certificateSignature;
    QString displayName;
    int colorIndex{0};
    int protocol{protocolVersion};
    int minProtocol{minSupportedProtocol};
};

// Whether a peer's versions overlap with this one's.
[[nodiscard]] bool protocolsCompatible(const Hello &hello);

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

// What the sender has of a board (a Yrs state vector), so the peer sends only the rest.
struct BoardState {
    QUuid board;
    QByteArray stateVector;
};

// One piece of a message too large to send whole (see message_parts.hpp).
struct MessagePart {
    quint32 index{0};
    quint32 count{0};
    QByteArray data;
};

// Presents a board link's token to one of the board's owners, asking to be admitted.
struct BoardLinkJoin {
    QUuid board;
    QByteArray token;
};

// Sent by an app instance that has just started, to the others on this device.
struct InstanceHello {};

// A message of a type this version does not know: from a newer one, and ignored.
struct UnknownMessage {};

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
    // Grows with every change, so a late datagram cannot undo a newer position.
    quint32 sequence{0};
};

// The newest part of a stroke someone is drawing, sent as a datagram. The finished
// stroke still reaches everyone through the board; this only shows it sooner.
inline constexpr std::size_t maxLiveStrokePoints = 64;
struct LiveStroke {
    QUuid board;
    quint32 stroke{0};     // identifies the stroke among the sender's
    quint32 firstPoint{0}; // index of points.front() within the stroke
    DraftState state{DraftState::Drawing};
    std::uint32_t argb{0};
    float width{1.0F};
    StrokeKind kind{StrokeKind::Pen};
    std::vector<StrokePoint> points; // at most maxLiveStrokePoints
};

using Message =
    std::variant<Hello, AccessChanges, Personal, BoardUpdate, Presence, Profile, LiveStroke,
                 BoardState, MessagePart, BoardLinkJoin, InstanceHello, UnknownMessage>;

[[nodiscard]] QByteArray encode(const Message &message);
[[nodiscard]] std::optional<Message> decode(const QByteArray &bytes);

} // namespace peerchalk::sync
