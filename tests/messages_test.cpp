#include "sync/messages.hpp"

#include <QJsonArray>

#include <gtest/gtest.h>

#include <cmath>

namespace peerchalk::sync {
namespace {

template <class T> T roundTrip(const Message &message) {
    const std::optional<Message> decoded = decode(encode(message));
    EXPECT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded && std::holds_alternative<T>(*decoded));
    return decoded ? std::get<T>(*decoded) : T{};
}

TEST(Messages, HelloRoundTrips) {
    const Hello hello = roundTrip<Hello>(Hello{"cert", "sig", QStringLiteral("Ana"), 3});
    EXPECT_EQ(hello.certificatePayload, QByteArray("cert"));
    EXPECT_EQ(hello.certificateSignature, QByteArray("sig"));
    EXPECT_EQ(hello.displayName, QStringLiteral("Ana"));
    EXPECT_EQ(hello.colorIndex, 3);
}

TEST(Messages, BoardUpdateRoundTrips) {
    const QUuid board = QUuid::createUuid();
    const BoardUpdate update =
        roundTrip<BoardUpdate>(BoardUpdate{board, QByteArray("\x01\x02", 2)});
    EXPECT_EQ(update.board, board);
    EXPECT_EQ(update.update, QByteArray("\x01\x02", 2));
}

TEST(Messages, PresenceAndProfileRoundTrip) {
    const QUuid board = QUuid::createUuid();
    const Presence presence = roundTrip<Presence>(Presence{board, {1.5, -2.0}, true});
    EXPECT_EQ(presence.board, board);
    EXPECT_DOUBLE_EQ(presence.cursor.x(), 1.5);
    EXPECT_TRUE(presence.cursorVisible);
    EXPECT_TRUE(roundTrip<Presence>(Presence{}).board.isNull());
    const Profile profile = roundTrip<Profile>(Profile{QStringLiteral("Bruno 1"), 2});
    EXPECT_EQ(profile.displayName, QStringLiteral("Bruno 1"));
    EXPECT_EQ(profile.colorIndex, 2);
}

TEST(Messages, AccessChangesAndPersonalRoundTrip) {
    const AccessChanges changes =
        roundTrip<AccessChanges>(AccessChanges{{{QByteArray("payload"), QByteArray("signature")}}});
    ASSERT_EQ(changes.ops.size(), 1U);
    EXPECT_EQ(changes.ops.front().first, QByteArray("payload"));
    EXPECT_EQ(changes.ops.front().second, QByteArray("signature"));
    const Personal personal =
        roundTrip<Personal>(Personal{{{"projects", QJsonArray()}}, {{"contacts", QJsonArray()}}});
    EXPECT_TRUE(personal.workspace.contains("projects"));
}

TEST(Messages, PresenceKeepsItsSequenceNumber) {
    EXPECT_EQ(roundTrip<Presence>(Presence{{}, {}, false, 4000000000U}).sequence, 4000000000U);
}

LiveStroke fullLiveStroke() {
    LiveStroke stroke{QUuid::createUuid(),     7, 100, DraftState::Finished, 0xFF102030U, 2.5F,
                      StrokeKind::Highlighter, {}};
    stroke.points.assign(maxLiveStrokePoints, StrokePoint{1.5F, -2.5F, 0.5F});
    return stroke;
}

TEST(Messages, LiveStrokesRoundTripWithinOneDatagram) {
    const LiveStroke stroke = fullLiveStroke();
    EXPECT_LE(encode(stroke).size(), 1024); // p2pdc::max_datagram_bytes
    const LiveStroke decoded = roundTrip<LiveStroke>(stroke);
    EXPECT_EQ(decoded.board, stroke.board);
    EXPECT_EQ(decoded.stroke, 7U);
    EXPECT_EQ(decoded.firstPoint, 100U);
    EXPECT_EQ(decoded.state, DraftState::Finished);
    EXPECT_EQ(decoded.argb, 0xFF102030U);
    EXPECT_FLOAT_EQ(decoded.width, 2.5F);
    EXPECT_EQ(decoded.kind, StrokeKind::Highlighter);
    ASSERT_EQ(decoded.points.size(), maxLiveStrokePoints);
    EXPECT_FLOAT_EQ(decoded.points.back().y, -2.5F);
}

TEST(Messages, RejectsMalformedLiveStrokes) {
    const auto rejects = [](auto change) {
        LiveStroke stroke = fullLiveStroke();
        change(stroke);
        return !decode(encode(stroke)).has_value();
    };
    EXPECT_TRUE(rejects([](LiveStroke &s) { s.board = QUuid(); }));
    EXPECT_TRUE(rejects([](LiveStroke &s) { s.width = 0.0F; }));
    EXPECT_TRUE(rejects([](LiveStroke &s) { s.width = std::nanf(""); }));
    EXPECT_TRUE(rejects([](LiveStroke &s) { s.points[3].pressure = 2.0F; }));
    EXPECT_TRUE(rejects([](LiveStroke &s) { s.points[3].x = std::nanf(""); }));
    EXPECT_TRUE(rejects([](LiveStroke &s) { s.points.emplace_back(s.points.front()); }));
    EXPECT_TRUE(rejects([](LiveStroke &s) { s.firstPoint = 0xFFFFFFFFU; }));
    QByteArray bytes = encode(fullLiveStroke());
    EXPECT_FALSE(decode(bytes.left(bytes.size() - 1)).has_value());
    EXPECT_FALSE(decode(bytes + 'x').has_value());
}

TEST(Messages, RejectsMalformedInput) {
    EXPECT_FALSE(decode({}).has_value());
    EXPECT_FALSE(decode(QByteArray("\x09{}", 3)).has_value());       // unknown type
    EXPECT_FALSE(decode(QByteArray("\x01not json", 9)).has_value()); // bad JSON
    EXPECT_FALSE(decode(QByteArray("\x04short", 6)).has_value());    // no board id
    EXPECT_FALSE(decode(QByteArray("\x01{\"certificate\":\"%%\"}")).has_value());
    EXPECT_FALSE(decode(QByteArray("\x05{\"board\":\"nope\"}")).has_value());
}

} // namespace
} // namespace peerchalk::sync
