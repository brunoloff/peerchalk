#pragma once

#include "board/stroke.hpp"
#include "sync/messages.hpp"

#include <QByteArray>
#include <QString>
#include <QUuid>

#include <chrono>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace peerchalk::sync {

// Puts together other people's unfinished strokes from LiveStroke datagrams, which may
// be lost, repeated or arrive in any order. These previews only bridge the time until
// the finished stroke arrives in the board itself, so they simply expire.
class LiveStrokeTracker {
  public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t maxStrokes = 256;
    static constexpr std::size_t maxPointsPerStroke = 20000;
    static constexpr std::chrono::milliseconds drawingLifetime{3000};
    static constexpr std::chrono::milliseconds finishedLifetime{1500};

    // What to change in a board's previews; without a stroke, the preview goes away.
    struct Change {
        QString key;
        QUuid board;
        std::optional<Stroke> stroke;
    };

    // Takes in a datagram from a device; returns the change it causes, if any.
    [[nodiscard]] std::optional<Change> apply(const QByteArray &device, const LiveStroke &message,
                                              Clock::time_point now);
    // Previews whose time is up.
    [[nodiscard]] std::vector<Change> expire(Clock::time_point now);
    // The device's finished strokes on a board, now that its board update has arrived.
    [[nodiscard]] std::vector<Change> settle(const QByteArray &device, const QUuid &board);
    [[nodiscard]] bool empty() const { return strokes_.empty(); }

  private:
    using Key = std::pair<QByteArray, quint32>;
    // A removed entry stays until it expires, so late datagrams cannot revive it.
    struct Entry {
        QUuid board;
        std::uint32_t argb{0};
        float width{1.0F};
        StrokeKind kind{StrokeKind::Pen};
        DraftState state{DraftState::Drawing};
        bool removed{false};
        Clock::time_point lastSeen;
        std::map<quint32, StrokePoint> points;
    };
    [[nodiscard]] static QString keyText(const Key &key);
    [[nodiscard]] static Change removal(const Key &key, const Entry &entry);
    [[nodiscard]] static Stroke strokeOf(const Entry &entry);

    std::map<Key, Entry> strokes_;
};

} // namespace peerchalk::sync
