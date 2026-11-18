#pragma once

#include "board/rich_text.hpp"

#include <cstddef>
#include <optional>

namespace peerchalk {

// Inserts an equation at index, on a line of its own if it is a display equation, and returns
// where it ended up.
std::size_t placeEquation(RichText &body, std::size_t index, RichUnit equation);
// If the character before end completes $...$, $$...$$ or \\[...\\], turns that into an
// equation and returns where the cursor belongs after it.
std::optional<std::size_t> convertTypedEquation(RichText &body, std::size_t end);

} // namespace peerchalk
