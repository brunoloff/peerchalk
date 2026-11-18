#include "sync/space_schedule.hpp"

#include <algorithm>
#include <tuple>

namespace peerchalk::sync {

std::vector<QString> chooseSpaces(std::vector<SpaceCandidate> candidates, std::size_t capacity,
                                  std::size_t rotatingSlots, std::uint64_t step) {
    std::ranges::sort(candidates, [](const SpaceCandidate &a, const SpaceCandidate &b) {
        if (std::tie(a.essential, a.connected) != std::tie(b.essential, b.connected))
            return std::tie(a.essential, a.connected) > std::tie(b.essential, b.connected);
        if (a.lastEdited != b.lastEdited)
            return a.lastEdited > b.lastEdited;
        return a.key < b.key;
    });
    const auto mustStay = static_cast<std::size_t>(
        std::ranges::count_if(candidates, [](const SpaceCandidate &candidate) {
            return candidate.essential || candidate.connected;
        }));
    const std::size_t fixed =
        std::min({candidates.size(), capacity,
                  std::max(mustStay, capacity - std::min(capacity, rotatingSlots))});
    std::vector<QString> chosen;
    chosen.reserve(std::min(candidates.size(), capacity));
    for (std::size_t i = 0; i < fixed; ++i)
        chosen.push_back(candidates[i].key);
    const std::size_t others = candidates.size() - fixed;
    const std::size_t turns = std::min(others, capacity - fixed);
    for (std::size_t i = 0; i < turns; ++i)
        chosen.push_back(candidates[fixed + (step + i) % others].key);
    return chosen;
}

} // namespace peerchalk::sync
