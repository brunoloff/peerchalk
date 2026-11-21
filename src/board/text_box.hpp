#pragma once

#include "board/geometry.hpp"
#include "board/rich_text.hpp"

#include <cstdint>
#include <string>

namespace peerchalk {

// A block of rich text on the board, in world coordinates. Its height follows from laying
// out the text, which only the interface can do.
struct TextBox {
    static constexpr float minWidth = 40.0F;
    static constexpr float maxWidth = 4000.0F;
    static constexpr float minFontSize = 6.0F;
    static constexpr float maxFontSize = 200.0F;

    std::string id;   // unique, chosen by whoever creates the box
    std::string page; // on page boards, the page it is on; x and y are then page coordinates
    float x{0.0F};
    float y{0.0F};
    float width{320.0F};
    float fontSize{20.0F};
    std::uint32_t argb{0xFF000000U};
    RichText body;
};

} // namespace peerchalk
