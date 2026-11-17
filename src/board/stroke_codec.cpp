#include "board/stroke_codec.hpp"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr std::uint8_t formatVersion = 1;
constexpr std::size_t headerSize = 14;
constexpr std::size_t pointSize = 12;

void putU32(Bytes &out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
}

void putF32(Bytes &out, float value) { putU32(out, std::bit_cast<std::uint32_t>(value)); }

std::uint32_t getU32(ByteView bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8 * i);
    return value;
}

float getF32(ByteView bytes, std::size_t offset) {
    return std::bit_cast<float>(getU32(bytes, offset));
}

} // namespace

Bytes encodeStroke(const Stroke &stroke) {
    const std::vector<StrokePoint> &points = stroke.points();
    if (points.size() > maxEncodedStrokePoints)
        throw std::length_error("Stroke has too many points to encode");
    Bytes out;
    out.reserve(headerSize + points.size() * pointSize);
    out.push_back(static_cast<std::byte>(formatVersion));
    out.push_back(static_cast<std::byte>(stroke.kind()));
    putU32(out, stroke.argb());
    putF32(out, stroke.width());
    putU32(out, static_cast<std::uint32_t>(points.size()));
    for (const StrokePoint &point : points) {
        putF32(out, point.x);
        putF32(out, point.y);
        putF32(out, point.pressure);
    }
    return out;
}

std::optional<Stroke> decodeStroke(ByteView bytes) {
    if (bytes.size() < headerSize || static_cast<std::uint8_t>(bytes[0]) != formatVersion)
        return std::nullopt;
    const std::uint32_t count = getU32(bytes, 10);
    if (count > maxEncodedStrokePoints || bytes.size() != headerSize + count * pointSize)
        return std::nullopt;
    try {
        Stroke stroke(getU32(bytes, 2), getF32(bytes, 6),
                      static_cast<StrokeKind>(static_cast<std::uint8_t>(bytes[1])));
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t offset = headerSize + i * pointSize;
            stroke.addPoint(
                {getF32(bytes, offset), getF32(bytes, offset + 4), getF32(bytes, offset + 8)});
        }
        return stroke;
    } catch (const std::invalid_argument &) {
        return std::nullopt; // values a Stroke rejects; documented as malformed input
    }
}

} // namespace peerchalk
