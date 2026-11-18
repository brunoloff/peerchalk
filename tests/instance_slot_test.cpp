#include "library/instance_slot.hpp"

#include <QDir>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

TEST(InstanceSlot, EveryInstanceGetsItsOwnPlace) {
    QTemporaryDir location;
    auto first = InstanceSlot::claim(location.path());
    auto second = InstanceSlot::claim(location.path());
    auto third = InstanceSlot::claim(location.path());
    EXPECT_EQ(first->root(), location.path());
    EXPECT_NE(second->root(), first->root());
    EXPECT_NE(third->root(), second->root());
    EXPECT_TRUE(QDir(third->root()).exists());
    ASSERT_EQ(third->others().size(), 2U);
    EXPECT_TRUE(third->others()[0].running);
    EXPECT_TRUE(third->othersRunning());
}

TEST(InstanceSlot, FindsAndRemovesWhatEndedInstancesLeft) {
    QTemporaryDir location;
    auto first = InstanceSlot::claim(location.path());
    QString endedRoot;
    {
        auto ended = InstanceSlot::claim(location.path());
        endedRoot = ended->root();
    }
    EXPECT_FALSE(first->othersRunning());
    first.reset();
    auto next = InstanceSlot::claim(location.path());
    ASSERT_EQ(next->index(), 0);
    ASSERT_EQ(next->others().size(), 1U);
    EXPECT_FALSE(next->others()[0].running);
    next->finishMerge(next->others());
    EXPECT_FALSE(QDir(endedRoot).exists());
    EXPECT_FALSE(next->othersRunning());
}

} // namespace
} // namespace peerchalk
