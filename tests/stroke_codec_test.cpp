#include "board/stroke_codec.hpp"

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

Stroke sample() {
    Stroke stroke(0x80FF0000U, 6.5F, StrokeKind::Highlighter);
    stroke.addPoint({-1.5F, 2.25F, 0.5F});
    stroke.addPoint({1e6F, -3.0F, 1.0F});
    return stroke;
}

TEST(StrokeCodec, RoundTripsEveryField) {
    const std::optional<Stroke> decoded = decodeStroke(encodeStroke(sample()));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->kind(), StrokeKind::Highlighter);
    EXPECT_EQ(decoded->argb(), 0x80FF0000U);
    EXPECT_FLOAT_EQ(decoded->width(), 6.5F);
    ASSERT_EQ(decoded->points().size(), 2U);
    EXPECT_FLOAT_EQ(decoded->points()[0].x, -1.5F);
    EXPECT_FLOAT_EQ(decoded->points()[1].x, 1e6F);
    EXPECT_FLOAT_EQ(decoded->points()[0].pressure, 0.5F);
}

TEST(StrokeCodec, IsLittleEndianWithAVersionByte) {
    const Bytes bytes = encodeStroke(sample());
    ASSERT_EQ(bytes.size(), 14U + 2U * 12U);
    EXPECT_EQ(bytes[0], std::byte{1});
    EXPECT_EQ(bytes[1], std::byte{1});
    EXPECT_EQ(bytes[2], std::byte{0x00});
    EXPECT_EQ(bytes[5], std::byte{0x80});
    EXPECT_EQ(bytes[10], std::byte{2});
}

TEST(StrokeCodec, RejectsTruncatedAndPaddedInput) {
    Bytes bytes = encodeStroke(sample());
    EXPECT_FALSE(decodeStroke(ByteView(bytes).first(bytes.size() - 1)).has_value());
    bytes.push_back(std::byte{0});
    EXPECT_FALSE(decodeStroke(bytes).has_value());
    EXPECT_FALSE(decodeStroke({}).has_value());
}

TEST(StrokeCodec, RejectsUnknownVersionAndKind) {
    Bytes bytes = encodeStroke(sample());
    bytes[0] = std::byte{2};
    EXPECT_FALSE(decodeStroke(bytes).has_value());
    bytes[0] = std::byte{1};
    bytes[1] = std::byte{7};
    EXPECT_FALSE(decodeStroke(bytes).has_value());
}

TEST(StrokeCodec, RejectsValuesAStrokeWouldRefuse) {
    Stroke stroke(0xFF000000U, 2.0F);
    stroke.addPoint({0.0F, 0.0F, 1.0F});
    Bytes bytes = encodeStroke(stroke);
    bytes[14 + 8 + 3] = std::byte{0x7F}; // pressure becomes a huge value
    EXPECT_FALSE(decodeStroke(bytes).has_value());
    Bytes zeroWidth = encodeStroke(stroke);
    for (int i = 6; i < 10; ++i)
        zeroWidth[static_cast<std::size_t>(i)] = std::byte{0};
    EXPECT_FALSE(decodeStroke(zeroWidth).has_value());
}

TEST(StrokeCodec, RejectsAHugePointCount) {
    Bytes bytes = encodeStroke(Stroke(0xFF000000U, 2.0F));
    bytes[13] = std::byte{0xFF};
    EXPECT_FALSE(decodeStroke(bytes).has_value());
}

} // namespace
} // namespace peerchalk
