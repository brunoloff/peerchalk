#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace peerchalk {

// Stands for an equation in text, as it does in Qt's text documents.
inline constexpr char16_t equationChar = 0xFFFC;
// Longer texts from peers are ignored, and local ones are refused.
inline constexpr std::size_t maxRichTextUnits = 64 * 1024;

// One UTF-16 code unit of formatted text, or one whole equation.
struct RichUnit {
    char16_t ch{0};
    bool bold{false};
    bool italic{false};
    bool equation{false};
    bool displayMath{false}; // equations only
    std::string tex;         // equations only, UTF-8

    bool operator==(const RichUnit &) const = default;
    [[nodiscard]] bool sameStyle(const RichUnit &other) const {
        return bold == other.bold && italic == other.italic;
    }
};

using RichText = std::vector<RichUnit>;

// Turning `before` into `after` replaces `removed` units at `start` with `added` new ones.
// When sameCharacters is set, removed == added and only the formatting differs.
struct RichChange {
    std::size_t start{0};
    std::size_t removed{0};
    std::size_t added{0};
    bool sameCharacters{false};

    [[nodiscard]] bool empty() const { return removed == 0 && added == 0; }
};

// The smallest single replacement; it never splits a surrogate pair.
[[nodiscard]] RichChange diffRichText(const RichText &before, const RichText &after);
[[nodiscard]] RichText plainRichText(std::u16string_view text);
[[nodiscard]] RichUnit equationUnit(std::string tex, bool displayMath);

// Conversions that replace anything invalid with U+FFFD.
[[nodiscard]] std::string toUtf8(std::u16string_view text);
[[nodiscard]] std::u16string toUtf16(std::string_view text);

} // namespace peerchalk
