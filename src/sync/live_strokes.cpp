#include "sync/live_strokes.hpp"

namespace peerchalk::sync {

QString LiveStrokeTracker::keyText(const Key &key) {
    return QString::fromLatin1(key.first.toHex()) + QLatin1Char(':') + QString::number(key.second);
}

LiveStrokeTracker::Change LiveStrokeTracker::removal(const Key &key, const Entry &entry) {
    return {keyText(key), entry.board, std::nullopt};
}

Stroke LiveStrokeTracker::strokeOf(const Entry &entry) {
    Stroke stroke(entry.argb, entry.width, entry.kind);
    for (const auto &[index, point] : entry.points)
        stroke.addPoint(point);
    return stroke;
}

std::optional<LiveStrokeTracker::Change> LiveStrokeTracker::apply(const QByteArray &device,
                                                                  const LiveStroke &message,
                                                                  Clock::time_point now) {
    const Key key{device, message.stroke};
    auto found = strokes_.find(key);
    if (found == strokes_.end()) {
        if (strokes_.size() >= maxStrokes ||
            (message.points.empty() && message.state != DraftState::Cancelled))
            return std::nullopt;
        found = strokes_.try_emplace(key).first;
        found->second.board = message.board;
    }
    Entry &entry = found->second;
    if (entry.removed || entry.board != message.board)
        return std::nullopt;
    entry.lastSeen = now;
    if (message.state == DraftState::Cancelled) {
        entry.removed = true;
        return removal(key, entry);
    }
    entry.argb = message.argb;
    entry.width = message.width;
    entry.kind = message.kind;
    if (message.state == DraftState::Finished)
        entry.state = DraftState::Finished;
    quint32 index = message.firstPoint;
    for (const StrokePoint &point : message.points) {
        if (entry.points.size() >= maxPointsPerStroke && !entry.points.contains(index))
            break;
        entry.points[index++] = point;
    }
    return Change{keyText(key), entry.board, strokeOf(entry)};
}

std::vector<LiveStrokeTracker::Change> LiveStrokeTracker::expire(Clock::time_point now) {
    std::vector<Change> changes;
    for (auto it = strokes_.begin(); it != strokes_.end();) {
        const Entry &entry = it->second;
        const auto lifetime =
            entry.state == DraftState::Drawing ? drawingLifetime : finishedLifetime;
        if (now - entry.lastSeen < lifetime) {
            ++it;
            continue;
        }
        if (!entry.removed)
            changes.push_back(removal(it->first, entry));
        it = strokes_.erase(it);
    }
    return changes;
}

std::vector<LiveStrokeTracker::Change> LiveStrokeTracker::settle(const QByteArray &device,
                                                                 const QUuid &board) {
    std::vector<Change> changes;
    for (auto &[key, entry] : strokes_) {
        if (key.first == device && entry.board == board && !entry.removed &&
            entry.state == DraftState::Finished) {
            entry.removed = true;
            changes.push_back(removal(key, entry));
        }
    }
    return changes;
}

} // namespace peerchalk::sync
