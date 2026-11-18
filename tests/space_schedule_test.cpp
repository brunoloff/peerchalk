#include "sync/space_schedule.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>

namespace peerchalk::sync {
namespace {

std::vector<SpaceCandidate> numbered(int count) {
    const QDateTime start = QDateTime::fromSecsSinceEpoch(1000000);
    std::vector<SpaceCandidate> result;
    for (int i = 0; i < count; ++i)
        result.push_back({QStringLiteral("s%1").arg(i), false, false, start.addSecs(-i)});
    return result;
}

bool has(const std::vector<QString> &keys, const char *key) {
    return std::ranges::find(keys, QString::fromLatin1(key)) != keys.end();
}

TEST(SpaceSchedule, EverythingFitsWhenThereIsRoom) {
    EXPECT_EQ(chooseSpaces(numbered(3), 16, 4, 9).size(), 3U);
}

TEST(SpaceSchedule, KeepsEssentialConnectedAndRecentSpaces) {
    auto spaces = numbered(30);
    spaces[29].essential = true;
    spaces[28].connected = true;
    const auto chosen = chooseSpaces(spaces, 16, 4, 0);
    EXPECT_EQ(chosen.size(), 16U);
    EXPECT_TRUE(has(chosen, "s29"));
    EXPECT_TRUE(has(chosen, "s28"));
    EXPECT_TRUE(has(chosen, "s0")); // the most recently edited
}

TEST(SpaceSchedule, EveryWaitingSpaceEventuallyGetsATurn) {
    std::set<QString> seen;
    std::vector<QString> previous;
    for (std::uint64_t step = 0; step < 18; ++step) {
        const auto chosen = chooseSpaces(numbered(30), 16, 4, step);
        ASSERT_EQ(std::set<QString>(chosen.begin(), chosen.end()).size(), 16U);
        seen.insert(chosen.begin(), chosen.end());
        if (step > 0) { // moving on replaces one space at a time
            const auto kept = std::ranges::count_if(chosen, [&](const QString &key) {
                return std::ranges::find(previous, key) != previous.end();
            });
            EXPECT_EQ(kept, 15);
        }
        previous = chosen;
    }
    EXPECT_EQ(seen.size(), 30U);
}

TEST(SpaceSchedule, NeverExceedsCapacityEvenIfAllAreEssential) {
    auto spaces = numbered(20);
    for (SpaceCandidate &space : spaces)
        space.essential = true;
    EXPECT_EQ(chooseSpaces(spaces, 16, 4, 3).size(), 16U);
}

} // namespace
} // namespace peerchalk::sync
