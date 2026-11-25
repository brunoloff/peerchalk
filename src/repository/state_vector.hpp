#pragma once

#include <QByteArray>

#include <cstdint>
#include <map>
#include <optional>

namespace peerchalk::repository {

// A Yrs state vector: for each client id, how many of its changes are known.
using StateVector = std::map<std::uint64_t, std::uint32_t>;

// Decodes Yrs's v1 encoding (a count, then pairs of client and clock, all variable-length
// unsigned integers); nullopt for anything malformed.
[[nodiscard]] std::optional<StateVector> decodeStateVector(const QByteArray &bytes);
[[nodiscard]] QByteArray encodeStateVector(const StateVector &vector);
// True when outer knows at least every change inner knows.
[[nodiscard]] bool covers(const StateVector &outer, const StateVector &inner);
// Everything either knows.
[[nodiscard]] StateVector merged(StateVector first, const StateVector &second);

} // namespace peerchalk::repository
