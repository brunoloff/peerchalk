#include "board/rich_text.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace peerchalk {
namespace {

constexpr char32_t replacement = 0xFFFD;

bool isHighSurrogate(char16_t unit) { return unit >= 0xD800 && unit <= 0xDBFF; }
bool isLowSurrogate(char16_t unit) { return unit >= 0xDC00 && unit <= 0xDFFF; }

void appendUtf8(std::string &out, char32_t code) {
    if (code < 0x80) {
        out += char(code);
    } else if (code < 0x800) {
        out += char(0xC0 | code >> 6);
        out += char(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        out += char(0xE0 | code >> 12);
        out += char(0x80 | (code >> 6 & 0x3F));
        out += char(0x80 | (code & 0x3F));
    } else {
        out += char(0xF0 | code >> 18);
        out += char(0x80 | (code >> 12 & 0x3F));
        out += char(0x80 | (code >> 6 & 0x3F));
        out += char(0x80 | (code & 0x3F));
    }
}

void appendUtf16(std::u16string &out, char32_t code) {
    if (code < 0x10000) {
        out += char16_t(code);
    } else {
        code -= 0x10000;
        out += char16_t(0xD800 + (code >> 10));
        out += char16_t(0xDC00 + (code & 0x3FF));
    }
}

// The code point starting at text[index], advancing index past it.
char32_t decodeUtf8(std::string_view text, std::size_t &index) {
    const auto lead = std::uint8_t(text[index++]);
    if (lead < 0x80)
        return lead;
    int extra = 0;
    char32_t code = 0;
    char32_t minimum = 0;
    if ((lead & 0xE0) == 0xC0) {
        extra = 1, code = lead & 0x1FU, minimum = 0x80;
    } else if ((lead & 0xF0) == 0xE0) {
        extra = 2, code = lead & 0x0FU, minimum = 0x800;
    } else if ((lead & 0xF8) == 0xF0) {
        extra = 3, code = lead & 0x07U, minimum = 0x10000;
    } else {
        return replacement;
    }
    for (; extra > 0; --extra) {
        if (index >= text.size() || (std::uint8_t(text[index]) & 0xC0) != 0x80)
            return replacement;
        code = code << 6 | (std::uint8_t(text[index++]) & 0x3FU);
    }
    const bool surrogate = code >= 0xD800 && code <= 0xDFFF;
    return code < minimum || code > 0x10FFFF || surrogate ? replacement : code;
}

bool sameCharacter(const RichUnit &a, const RichUnit &b) {
    return a.ch == b.ch && a.equation == b.equation && a.displayMath == b.displayMath &&
           a.tex == b.tex;
}

} // namespace

RichChange diffRichText(const RichText &before, const RichText &after) {
    const std::size_t shorter = std::min(before.size(), after.size());
    std::size_t start = 0;
    while (start < shorter && before[start] == after[start])
        ++start;
    if (start > 0 && start < before.size() && isLowSurrogate(before[start].ch))
        --start;
    else if (start > 0 && start < after.size() && isLowSurrogate(after[start].ch))
        --start;
    std::size_t tail = 0;
    while (tail < shorter - start &&
           before[before.size() - 1 - tail] == after[after.size() - 1 - tail])
        ++tail;
    const auto splitsPair = [&](const RichText &text) {
        const std::size_t end = text.size() - tail;
        return tail > 0 && end > start && isHighSurrogate(text[end - 1].ch);
    };
    if (splitsPair(before) || splitsPair(after))
        --tail;
    RichChange change{start, before.size() - start - tail, after.size() - start - tail, false};
    change.sameCharacters =
        change.removed == change.added && change.removed > 0 &&
        std::equal(before.begin() + std::ptrdiff_t(start),
                   before.begin() + std::ptrdiff_t(start + change.removed),
                   after.begin() + std::ptrdiff_t(start), sameCharacter);
    return change;
}

RichText plainRichText(std::u16string_view text) {
    RichText result(text.size());
    std::transform(text.begin(), text.end(), result.begin(), [](char16_t ch) {
        RichUnit unit;
        unit.ch = ch;
        return unit;
    });
    return result;
}

RichUnit equationUnit(std::string tex, bool displayMath) {
    RichUnit unit;
    unit.ch = equationChar;
    unit.equation = true;
    unit.displayMath = displayMath;
    unit.tex = std::move(tex);
    return unit;
}

std::string toUtf8(std::u16string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char16_t unit = text[i];
        if (isHighSurrogate(unit) && i + 1 < text.size() && isLowSurrogate(text[i + 1])) {
            appendUtf8(out, 0x10000 + ((char32_t(unit) - 0xD800) << 10) +
                                (char32_t(text[++i]) - 0xDC00));
        } else {
            const bool lone = isHighSurrogate(unit) || isLowSurrogate(unit);
            appendUtf8(out, lone ? replacement : unit);
        }
    }
    return out;
}

std::u16string toUtf16(std::string_view text) {
    std::u16string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();)
        appendUtf16(out, decodeUtf8(text, i));
    return out;
}

} // namespace peerchalk
