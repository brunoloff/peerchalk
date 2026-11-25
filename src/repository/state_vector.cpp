#include "repository/state_vector.hpp"

#include <algorithm>
#include <limits>

namespace peerchalk::repository {
namespace {

constexpr int maxVarintBytes = 10;

std::optional<std::uint64_t> readVarint(const QByteArray &bytes, qsizetype &position) {
    std::uint64_t value = 0;
    for (int index = 0; index < maxVarintBytes; ++index) {
        if (position >= bytes.size())
            return std::nullopt;
        const auto byte = static_cast<std::uint8_t>(bytes[position++]);
        value |= static_cast<std::uint64_t>(byte & 0x7fU) << (7 * index);
        if ((byte & 0x80U) == 0)
            return value;
    }
    return std::nullopt;
}

void writeVarint(QByteArray &bytes, std::uint64_t value) {
    while (value >= 0x80U) {
        bytes.append(static_cast<char>((value & 0x7fU) | 0x80U));
        value >>= 7;
    }
    bytes.append(static_cast<char>(value));
}

} // namespace

std::optional<StateVector> decodeStateVector(const QByteArray &bytes) {
    qsizetype position = 0;
    const std::optional<std::uint64_t> count = readVarint(bytes, position);
    if (!count || *count > static_cast<std::uint64_t>(bytes.size()))
        return std::nullopt;
    StateVector vector;
    for (std::uint64_t index = 0; index < *count; ++index) {
        const std::optional<std::uint64_t> client = readVarint(bytes, position);
        const std::optional<std::uint64_t> clock = readVarint(bytes, position);
        if (!client || !clock || *clock > std::numeric_limits<std::uint32_t>::max())
            return std::nullopt;
        vector[*client] = static_cast<std::uint32_t>(*clock);
    }
    if (position != bytes.size())
        return std::nullopt;
    return vector;
}

QByteArray encodeStateVector(const StateVector &vector) {
    QByteArray bytes;
    writeVarint(bytes, vector.size());
    for (const auto &[client, clock] : vector) {
        writeVarint(bytes, client);
        writeVarint(bytes, clock);
    }
    return bytes;
}

bool covers(const StateVector &outer, const StateVector &inner) {
    return std::ranges::all_of(inner, [&](const auto &entry) {
        const auto found = outer.find(entry.first);
        return entry.second == 0 || (found != outer.end() && found->second >= entry.second);
    });
}

StateVector merged(StateVector first, const StateVector &second) {
    for (const auto &[client, clock] : second)
        first[client] = std::max(first[client], clock);
    return first;
}

} // namespace peerchalk::repository
