#include "board/page_layout.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace peerchalk {
namespace {

ColumnItem page(const char *id) { return {id, ColumnItemKind::Page, std::nullopt}; }
ColumnItem subheading(const char *id) { return {id, ColumnItemKind::Subheading, std::nullopt}; }

TEST(PaperSizes, MatchRealPaperInPoints) {
    // PDFs measure in points, 72 per inch; board units are 96 per inch.
    EXPECT_NEAR(paperA4.width * 0.75F, 595.28F, 0.01F);
    EXPECT_NEAR(paperA4.height * 0.75F, 841.89F, 0.01F);
    EXPECT_FLOAT_EQ(paperLetter.width * 0.75F, 612.0F);
    EXPECT_FLOAT_EQ(landscape(paperLetter).width, paperLetter.height);
}

TEST(PageLayout, StacksItemsBelowTheHeadingWithGaps) {
    const LayoutMetrics m;
    const PageLayout layout =
        layoutPages({{"c", paperLetter, {subheading("h"), page("p1"), page("p2")}}});
    ASSERT_EQ(layout.columns.size(), 1U);
    const PlacedColumn &column = layout.columns[0];
    EXPECT_FLOAT_EQ(column.heading.bottom, m.headingHeight);
    const float subheadingTop = m.headingHeight + m.itemGap;
    EXPECT_FLOAT_EQ(column.items[0].bounds.top, subheadingTop);
    EXPECT_FLOAT_EQ(column.items[1].bounds.top, subheadingTop + m.subheadingHeight + m.itemGap);
    EXPECT_FLOAT_EQ(column.items[2].bounds.top, column.items[1].bounds.bottom + m.itemGap);
    EXPECT_FLOAT_EQ(column.items[2].bounds.height(), paperLetter.height);
    ASSERT_EQ(column.slots.size(), 4U); // before each item and after the last
    EXPECT_FLOAT_EQ(column.slots[3].bounds.top, column.items[2].bounds.bottom);
    EXPECT_FLOAT_EQ(column.bounds.bottom, column.slots[3].bounds.bottom);
}

TEST(PageLayout, PlacesColumnsLeftToRightWithSlotsAroundThem) {
    const LayoutMetrics m;
    const PageLayout layout =
        layoutPages({{"a", paperA4, {page("p")}}, {"b", landscape(paperLetter), {page("q")}}});
    EXPECT_FLOAT_EQ(layout.columns[0].bounds.left, 0.0F);
    EXPECT_FLOAT_EQ(layout.columns[1].bounds.left, paperA4.width + m.columnGap);
    EXPECT_FLOAT_EQ(layout.columns[1].bounds.width(), paperLetter.height);
    ASSERT_EQ(layout.columnSlots.size(), 3U);
    EXPECT_FLOAT_EQ(layout.columnSlots[1].bounds.left, layout.columns[0].bounds.right);
    EXPECT_FLOAT_EQ(layout.columnSlots[1].bounds.right, layout.columns[1].bounds.left);
    EXPECT_FLOAT_EQ(layout.columnSlots[2].bounds.left, layout.columns[1].bounds.right);
}

TEST(PageLayout, WidensAColumnForAWiderPageAndCentresTheOthers) {
    const PaperSize wide{1200.0F, 700.0F};
    const PageLayout layout =
        layoutPages({{"c", paperA4, {page("p"), {"wide", ColumnItemKind::Page, wide}}}});
    const PlacedColumn &column = layout.columns[0];
    EXPECT_FLOAT_EQ(column.bounds.width(), wide.width);
    EXPECT_FLOAT_EQ(column.items[0].bounds.left, (wide.width - paperA4.width) / 2.0F);
    EXPECT_FLOAT_EQ(column.items[1].bounds.height(), wide.height);
}

TEST(PageLayout, AnEmptyBoardStillOffersAPlaceForAColumn) {
    const PageLayout layout = layoutPages({});
    EXPECT_TRUE(layout.columns.empty());
    ASSERT_EQ(layout.columnSlots.size(), 1U);
    EXPECT_FLOAT_EQ(layout.columnSlots[0].bounds.left, 0.0F);
}

TEST(PageLayout, RejectsSizesThatAreNotPositive) {
    EXPECT_THROW((void)layoutPages({{"c", {0.0F, 100.0F}, {}}}), std::invalid_argument);
    const PaperSize nan{std::numeric_limits<float>::quiet_NaN(), 10.0F};
    EXPECT_THROW((void)layoutPages({{"c", paperA4, {{"p", ColumnItemKind::Page, nan}}}}),
                 std::invalid_argument);
}

TEST(PageLayout, FindsTheItemsInAnAreaAndThePageUnderAPoint) {
    std::vector<ColumnItem> items;
    for (int i = 0; i < 300; ++i)
        items.push_back(page("p"));
    const PageLayout layout = layoutPages({{"a", paperA4, items}, {"b", paperA4, {page("q")}}});
    const PlacedItem &hundredth = layout.columns[0].items[100];
    const Bounds area{10.0F, hundredth.bounds.top + 10.0F, 20.0F, hundredth.bounds.bottom + 50.0F};
    const std::vector<ItemRef> found = itemsIn(layout, area);
    ASSERT_EQ(found.size(), 2U); // reaches into the next page, not the other column
    EXPECT_EQ(found[0], (ItemRef{0, 100}));
    EXPECT_EQ(found[1], (ItemRef{0, 101}));
    EXPECT_EQ(pageAt(layout, {10.0F, hundredth.bounds.top + 1.0F}), (ItemRef{0, 100}));
    EXPECT_FALSE(pageAt(layout, {10.0F, hundredth.bounds.bottom + 1.0F})); // in the gap
    EXPECT_FALSE(pageAt(layout, {paperA4.width + 10.0F, 500.0F}));         // between columns
}

} // namespace
} // namespace peerchalk
