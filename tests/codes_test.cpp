#include "library/codes.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace peerchalk {
namespace {

QByteArray filled(qsizetype size, char value) { return QByteArray(size, value); }

TEST(Codes, InviteRoundTrips) {
    const Invite invite{Invite::Kind::Board, QUuid::createUuid(), QStringLiteral("Thesis — group"),
                        filled(32, 7), filled(32, 9)};
    const QString text = encodeInvite(invite);
    EXPECT_TRUE(text.startsWith(QStringLiteral("peerchalk-invite:2:")));
    const std::optional<Invite> decoded = decodeInvite(QStringLiteral("  ") + text + "\n");
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->kind, Invite::Kind::Board);
    EXPECT_EQ(decoded->id, invite.id);
    EXPECT_EQ(decoded->name, invite.name);
    EXPECT_EQ(decoded->capability, invite.capability);
    EXPECT_EQ(decoded->admin, invite.admin);
}

TEST(Codes, ContactCardRoundTrips) {
    const ContactCard card{filled(32, 3), QStringLiteral("Ana")};
    const std::optional<ContactCard> decoded = decodeContactCard(encodeContactCard(card));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->signingKey, card.signingKey);
    EXPECT_EQ(decoded->name, card.name);
}

TEST(Codes, LongNamesAreCutAtACharacterBoundary) {
    const QString name(150, QChar(0x00E9)); // two UTF-8 bytes each
    const std::optional<ContactCard> decoded =
        decodeContactCard(encodeContactCard({filled(32, 1), name}));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->name, QString(100, QChar(0x00E9)));
}

TEST(Codes, RejectsMalformedText) {
    const QString card = encodeContactCard({filled(32, 1), QStringLiteral("Kofi")});
    EXPECT_FALSE(decodeContactCard(QStringLiteral("hello")).has_value());
    EXPECT_FALSE(decodeContactCard(card.left(card.indexOf(':', 18) + 20)).has_value());
    EXPECT_FALSE(decodeContactCard(card + QStringLiteral("!!")).has_value());
    EXPECT_FALSE(decodeInvite(card).has_value());
    EXPECT_FALSE(decodeContactCard(QStringLiteral("peerchalk-contact:1:")).has_value());
}

TEST(Codes, RejectsInvalidUtf8Names) {
    QByteArray bytes = filled(32, 1) + QByteArray("\xC3\x28", 2);
    const QString text = QStringLiteral("peerchalk-contact:1:") +
                         QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding |
                                                            QByteArray::OmitTrailingEquals));
    EXPECT_FALSE(decodeContactCard(text).has_value());
}

TEST(Codes, EncodingRejectsBadInput) {
    EXPECT_THROW(static_cast<void>(encodeContactCard({filled(31, 1), QStringLiteral("A")})),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(encodeContactCard({filled(32, 1), QStringLiteral("  ")})),
                 std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(encodeInvite(
            {Invite::Kind::Project, QUuid(), QStringLiteral("P"), filled(32, 1), filled(32, 2)})),
        std::invalid_argument);
}

TEST(Codes, FingerprintIsGroupedHex) {
    EXPECT_EQ(formatFingerprint(QByteArray::fromHex("1a2b3c4d5e6f")),
              QStringLiteral("1A2B 3C4D 5E6F"));
}

} // namespace
} // namespace peerchalk
