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

TEST(Messages, PresenceCarriesTheVisibleArea) {
    const QRectF view(-10.5, 20.0, 800.0, 600.0);
    const Presence presence =
        roundTrip<Presence>(Presence{QUuid::createUuid(), {1.0, 2.0}, true, 3, view});
    EXPECT_EQ(presence.view, view);
    EXPECT_FALSE(roundTrip<Presence>(Presence{}).view.isValid());
    EXPECT_FALSE(decode(QByteArray("\x05{\"view\":[1,2,0,4]}")).has_value()); // empty
    EXPECT_FALSE(decode(QByteArray("\x05{\"view\":[1,2,3]}")).has_value());
}

TEST(Messages, TypesFromNewerVersionsAreRecognisedAsUnknown) {
    const auto decoded = decode(QByteArray("\x7f"
                                           "anything",
                                           9));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(std::holds_alternative<UnknownMessage>(*decoded));
}

TEST(Messages, HelloCarriesProtocolVersions) {
    const Hello hello = roundTrip<Hello>(Hello{"c", "s", QStringLiteral("Ana"), 0, 7, 2});
    EXPECT_EQ(hello.protocol, 7);
    EXPECT_EQ(hello.minProtocol, 2);
    EXPECT_TRUE(protocolsCompatible(hello));
}

TEST(Messages, VersionsMustOverlap) {
    EXPECT_FALSE(protocolsCompatible(Hello{{}, {}, {}, 0, minSupportedProtocol - 1, 1}));
    EXPECT_FALSE(
        protocolsCompatible(Hello{{}, {}, {}, 0, protocolVersion + 2, protocolVersion + 1}));
    // A Hello without versions comes from before they existed.
    const auto old = decode(QByteArray("\x01{\"certificate\":\"\",\"signature\":\"\"}"));
    ASSERT_TRUE(old.has_value());
    EXPECT_FALSE(protocolsCompatible(std::get<Hello>(*old)));
}

TEST(Messages, BoardStatesAndPartsRoundTrip) {
    const QUuid board = QUuid::createUuid();
    const BoardState state = roundTrip<BoardState>(BoardState{board, "vector"});
    EXPECT_EQ(state.board, board);
    EXPECT_EQ(state.stateVector, QByteArray("vector"));
    const MessagePart part = roundTrip<MessagePart>(MessagePart{1, 3, "data"});
    EXPECT_EQ(part.index, 1U);
    EXPECT_EQ(part.count, 3U);
    EXPECT_EQ(part.data, QByteArray("data"));
    EXPECT_FALSE(decode(encode(MessagePart{3, 3, "data"})).has_value());
}

TEST(Messages, BoardLinkJoinsRoundTripAndNeedAWholeToken) {
    const QUuid board = QUuid::createUuid();
    const QByteArray token(32, 't');
    const BoardLinkJoin join = roundTrip<BoardLinkJoin>(BoardLinkJoin{board, token});
    EXPECT_EQ(join.board, board);
    EXPECT_EQ(join.token, token);
    EXPECT_FALSE(decode(encode(BoardLinkJoin{board, "short"})).has_value());
}

TEST(Messages, BlobRequestsAndPiecesRoundTripWithinTheirLimits) {
    const QByteArray hash(blobHashBytes, 'h');
    const BlobWant want = roundTrip<BlobWant>(BlobWant{hash, 1024});
    EXPECT_EQ(want.hash, hash);
    EXPECT_EQ(want.offset, 1024U);
    const BlobChunk chunk = roundTrip<BlobChunk>(BlobChunk{hash, 4, 10, "abcdef"});
    EXPECT_EQ(chunk.offset, 4U);
    EXPECT_EQ(chunk.total, 10U);
    EXPECT_EQ(chunk.data, QByteArray("abcdef"));
    EXPECT_FALSE(decode(encode(BlobWant{"short", 0})).has_value());
    EXPECT_FALSE(decode(encode(BlobChunk{hash, 8, 10, "abcdef"})).has_value()); // past the end
    EXPECT_FALSE(decode(encode(BlobChunk{hash, 0, maxBlobBytes + 1, "a"})).has_value());
    EXPECT_FALSE(decode(encode(BlobChunk{hash, 0, 10, ""})).has_value()); // no data
}

TEST(Messages, RejectsMalformedInput) {
    EXPECT_FALSE(decode({}).has_value());
    EXPECT_FALSE(decode(QByteArray("\x09{}", 3)).has_value());       // board state, no id
    EXPECT_FALSE(decode(QByteArray("\x08\0\0", 3)).has_value());     // part without a header
    EXPECT_FALSE(decode(QByteArray("\x01not json", 9)).has_value()); // bad JSON
    EXPECT_FALSE(decode(QByteArray("\x04short", 6)).has_value());    // no board id
    EXPECT_FALSE(decode(QByteArray("\x01{\"certificate\":\"%%\"}")).has_value());
    EXPECT_FALSE(decode(QByteArray("\x05{\"board\":\"nope\"}")).has_value());
}

} // namespace
} // namespace peerchalk::sync
