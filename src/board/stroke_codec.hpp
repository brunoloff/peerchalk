#pragma once

#include "board/stroke.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace peerchalk {

using Bytes = std::vector<std::byte>;
using ByteView = std::span<const std::byte>;

// Most points one encoded stroke may hold; longer encodings are rejected.
inline constexpr std::size_t maxEncodedStrokePoints = 100000;

// Encodes a stroke in PeerChalk's stroke format, version 1 (all values little-endian):
//   u8 version (1), u8 kind, u32 argb, f32 width, u32 point count,
//   then per point: f32 x, f32 y, f32 pressure.
// Throws std::length_error for strokes with more than maxEncodedStrokePoints points.
[[nodiscard]] Bytes encodeStroke(const Stroke &stroke);

// Decodes a stroke written by encodeStroke. Returns nullopt for anything malformed:
// wrong version or length, unknown kind, or values a Stroke would reject. Peers send
// these bytes, so they are untrusted.
[[nodiscard]] std::optional<Stroke> decodeStroke(ByteView bytes);

} // namespace peerchalk
