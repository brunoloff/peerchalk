#include "sync/messages.hpp"

#include <QJsonArray>

#include <gtest/gtest.h>

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

TEST(Messages, ManifestsAndPersonalRoundTrip) {
    const Manifests manifests =
        roundTrip<Manifests>(Manifests{{{{}, QByteArray("payload"), QByteArray("signature")}}});
    ASSERT_EQ(manifests.manifests.size(), 1U);
    EXPECT_EQ(manifests.manifests.front().payload, QByteArray("payload"));
    const Personal personal =
        roundTrip<Personal>(Personal{{{"projects", QJsonArray()}}, {{"contacts", QJsonArray()}}});
    EXPECT_TRUE(personal.workspace.contains("projects"));
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
