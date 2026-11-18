#include "sync/message_parts.hpp"

#include <gtest/gtest.h>

namespace peerchalk::sync {
namespace {

using Status = MessageAssembler::Status;

MessagePart decodedPart(const QByteArray &encoded) {
    return std::get<MessagePart>(decode(encoded).value());
}

TEST(MessageParts, SmallMessagesStayWhole) {
    const QByteArray message(maxPartBytes, 'x');
    EXPECT_EQ(splitMessage(message), std::vector<QByteArray>{message});
}

TEST(MessageParts, LargeMessagesAreRebuiltExactly) {
    QByteArray message(2 * maxPartBytes + 17, '\0');
    for (qsizetype i = 0; i < message.size(); ++i)
        message[i] = static_cast<char>(i * 31);
    const std::vector<QByteArray> parts = splitMessage(message);
    ASSERT_EQ(parts.size(), 3U);
    MessageAssembler assembler;
    EXPECT_EQ(assembler.add(decodedPart(parts[0]), maxMessageBytes), Status::Incomplete);
    EXPECT_EQ(assembler.add(decodedPart(parts[1]), maxMessageBytes), Status::Incomplete);
    EXPECT_EQ(assembler.add(decodedPart(parts[2]), maxMessageBytes), Status::Complete);
    EXPECT_EQ(assembler.take(), message);
}

TEST(MessageParts, RefusesPartsOutOfOrder) {
    const auto parts = splitMessage(QByteArray(2 * maxPartBytes + 1, 'x'));
    MessageAssembler assembler;
    EXPECT_EQ(assembler.add(decodedPart(parts[1]), maxMessageBytes), Status::Invalid);
    ASSERT_EQ(assembler.add(decodedPart(parts[0]), maxMessageBytes), Status::Incomplete);
    EXPECT_EQ(assembler.add(decodedPart(parts[2]), maxMessageBytes), Status::Invalid);
}

TEST(MessageParts, RefusesAChangedPartCountAndShortMiddleParts) {
    const QByteArray full(maxPartBytes, 'x');
    MessageAssembler assembler;
    ASSERT_EQ(assembler.add({0, 3, full}, maxMessageBytes), Status::Incomplete);
    EXPECT_EQ(assembler.add({1, 4, full}, maxMessageBytes), Status::Invalid);
    EXPECT_EQ(assembler.add({0, 3, QByteArray("short")}, maxMessageBytes), Status::Invalid);
}

TEST(MessageParts, StopsAtTheLimitBeforeStoringMore) {
    const QByteArray full(maxPartBytes, 'x');
    MessageAssembler assembler;
    ASSERT_EQ(assembler.add({0, 9, full}, maxPartBytes), Status::Incomplete);
    EXPECT_EQ(assembler.add({1, 9, full}, maxPartBytes), Status::Invalid);
}

} // namespace
} // namespace peerchalk::sync
