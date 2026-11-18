#include "sync/live_strokes.hpp"

#include <gtest/gtest.h>

namespace peerchalk::sync {
namespace {

using Clock = LiveStrokeTracker::Clock;

const QByteArray device("device");
const QUuid board = QUuid::createUuid();
const Clock::time_point start{};

LiveStroke message(quint32 first, std::vector<float> xs, DraftState state = DraftState::Drawing,
                   quint32 stroke = 1) {
    LiveStroke result{board, stroke, first, state, 0xFF000000U, 2.0F, StrokeKind::Pen, {}};
    for (float x : xs)
        result.points.push_back({x, 0.0F, 1.0F});
    return result;
}

std::vector<float> xsOf(const Stroke &stroke) {
    std::vector<float> xs;
    for (const StrokePoint &point : stroke.points())
        xs.push_back(point.x);
    return xs;
}

TEST(LiveStrokeTracker, OverlappingAndReorderedPartsCombine) {
    LiveStrokeTracker tracker;
    ASSERT_TRUE(tracker.apply(device, message(2, {3.0F, 4.0F}), start));
    const auto change = tracker.apply(device, message(0, {1.0F, 2.0F, 3.0F}), start);
    ASSERT_TRUE(change && change->stroke);
    EXPECT_EQ(change->board, board);
    EXPECT_EQ(xsOf(*change->stroke), (std::vector<float>{1.0F, 2.0F, 3.0F, 4.0F}));
}

TEST(LiveStrokeTracker, MissingPartsLeaveAStraightJoin) {
    LiveStrokeTracker tracker;
    ASSERT_TRUE(tracker.apply(device, message(0, {1.0F}), start));
    const auto change = tracker.apply(device, message(10, {9.0F}), start);
    EXPECT_EQ(xsOf(*change->stroke), (std::vector<float>{1.0F, 9.0F}));
}

TEST(LiveStrokeTracker, DevicesAndStrokesStaySeparate) {
    LiveStrokeTracker tracker;
    const auto first = tracker.apply(device, message(0, {1.0F}), start);
    const auto second = tracker.apply("other", message(0, {2.0F}), start);
    const auto third = tracker.apply(device, message(0, {3.0F}, DraftState::Drawing, 2), start);
    EXPECT_NE(first->key, second->key);
    EXPECT_NE(first->key, third->key);
}

TEST(LiveStrokeTracker, CancelledStrokesGoAwayAndStayAway) {
    LiveStrokeTracker tracker;
    ASSERT_TRUE(tracker.apply(device, message(0, {1.0F}), start));
    const auto cancelled = tracker.apply(device, message(0, {}, DraftState::Cancelled), start);
    ASSERT_TRUE(cancelled);
    EXPECT_FALSE(cancelled->stroke.has_value());
    EXPECT_FALSE(tracker.apply(device, message(1, {2.0F}), start).has_value());
}

TEST(LiveStrokeTracker, FinishedStrokesSettleWhenTheBoardCatchesUp) {
    LiveStrokeTracker tracker;
    ASSERT_TRUE(tracker.apply(device, message(0, {1.0F}), start));
    EXPECT_TRUE(tracker.settle(device, board).empty()); // still being drawn
    ASSERT_TRUE(tracker.apply(device, message(0, {1.0F}, DraftState::Finished), start));
    EXPECT_TRUE(tracker.settle("other", board).empty());
    const auto changes = tracker.settle(device, board);
    ASSERT_EQ(changes.size(), 1U);
    EXPECT_FALSE(changes.front().stroke.has_value());
    EXPECT_TRUE(tracker.settle(device, board).empty());
}

TEST(LiveStrokeTracker, SilentStrokesExpire) {
    LiveStrokeTracker tracker;
    ASSERT_TRUE(tracker.apply(device, message(0, {1.0F}), start));
    EXPECT_TRUE(tracker.expire(start + LiveStrokeTracker::drawingLifetime / 2).empty());
    EXPECT_EQ(tracker.expire(start + LiveStrokeTracker::drawingLifetime).size(), 1U);
    EXPECT_TRUE(tracker.empty());
}

TEST(LiveStrokeTracker, FinishedStrokesExpireSooner) {
    LiveStrokeTracker tracker;
    ASSERT_TRUE(tracker.apply(device, message(0, {1.0F}, DraftState::Finished), start));
    EXPECT_EQ(tracker.expire(start + LiveStrokeTracker::finishedLifetime).size(), 1U);
}

TEST(LiveStrokeTracker, LimitsWhatPeersCanMakeItKeep) {
    LiveStrokeTracker tracker;
    for (quint32 i = 0; i < LiveStrokeTracker::maxStrokes; ++i)
        ASSERT_TRUE(tracker.apply(device, message(0, {1.0F}, DraftState::Drawing, i), start));
    EXPECT_FALSE(tracker.apply(device, message(0, {1.0F}, DraftState::Drawing, 9999), start));

    LiveStrokeTracker points;
    for (quint32 i = 0; i < LiveStrokeTracker::maxPointsPerStroke + 5; ++i)
        (void)points.apply(device, message(i, {1.0F}), start);
    const auto change = points.apply(device, message(0, {2.0F}), start);
    EXPECT_EQ(change->stroke->points().size(), LiveStrokeTracker::maxPointsPerStroke);
}

TEST(LiveStrokeTracker, IgnoresEmptyMessagesForUnknownStrokes) {
    LiveStrokeTracker tracker;
    EXPECT_FALSE(tracker.apply(device, message(0, {}), start).has_value());
    EXPECT_FALSE(tracker.apply(device, message(0, {}, DraftState::Finished), start).has_value());
    EXPECT_TRUE(tracker.empty());
}

} // namespace
} // namespace peerchalk::sync
