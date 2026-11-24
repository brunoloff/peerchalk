#include "sync/stun_message.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace peerchalk::sync {
namespace {

const QByteArray transaction = QByteArray::fromHex("0102030405060708090a0b0c");

TEST(StunMessageTest, BuildsABindingRequestWithTheMagicCookie) {
    const QByteArray request = stunBindingRequest(transaction);
    EXPECT_EQ(request.toHex(), "000100002112a442" + transaction.toHex());
    EXPECT_THROW(static_cast<void>(stunBindingRequest("short")), std::invalid_argument);
}

TEST(StunMessageTest, RecognisesAnswersToThatTransactionOnly) {
    // A success response with one 12-byte XOR-MAPPED-ADDRESS attribute.
    const QByteArray success = QByteArray::fromHex("0101000c2112a442") + transaction +
                               QByteArray::fromHex("0020000800012a5c5e12a443");
    EXPECT_TRUE(isStunBindingAnswer(success, transaction));
    QByteArray error = success;
    error[1] = 0x11; // a Binding error response still shows the server answers
    EXPECT_TRUE(isStunBindingAnswer(error, transaction));
    EXPECT_FALSE(isStunBindingAnswer(success, QByteArray::fromHex("ffffffffffffffffffffffff")));
    EXPECT_FALSE(isStunBindingAnswer(success.left(24), transaction)) << "length disagrees";
    QByteArray request = stunBindingRequest(transaction);
    EXPECT_FALSE(isStunBindingAnswer(request, transaction)) << "a request is no answer";
}

} // namespace
} // namespace peerchalk::sync
