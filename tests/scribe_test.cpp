#include "sync/scribe.hpp"

#include <gtest/gtest.h>

namespace peerchalk::sync {
namespace {

TEST(Scribe, TheLowestMemberDeviceThatCanPushWrites) {
    const QByteArray a(32, 'a');
    const QByteArray b(32, 'b');
    const QByteArray c(32, 'c');
    EXPECT_EQ(electScribe({{c, true, true}, {b, true, true}}), b);
    EXPECT_EQ(electScribe({{a, true, false}, {c, true, true}}), c); // a cannot push
    EXPECT_EQ(electScribe({{a, false, true}, {c, true, true}}), c); // a is no member
    EXPECT_FALSE(electScribe({{a, true, false}, {b, false, true}}).has_value());
    EXPECT_FALSE(electScribe({}).has_value());
}

TEST(Scribe, EveryDeviceAgreesWhateverTheOrder) {
    const std::vector<ScribeCandidate> one{{QByteArray(32, 'x'), true, true},
                                           {QByteArray(32, 'm'), true, true},
                                           {QByteArray(32, 'q'), true, true}};
    const std::vector<ScribeCandidate> other(one.rbegin(), one.rend());
    EXPECT_EQ(electScribe(one), electScribe(other));
    EXPECT_EQ(electScribe(one), QByteArray(32, 'm'));
}

} // namespace
} // namespace peerchalk::sync
