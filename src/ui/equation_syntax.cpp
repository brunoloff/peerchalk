#include "ui/equation_syntax.hpp"

#include <algorithm>
#include <utility>

namespace peerchalk {
namespace {

bool isDollar(const RichUnit &unit) { return !unit.equation && unit.ch == u'$'; }

struct EquationSpan {
    std::size_t first;
    std::u16string tex;
    bool block;
};

std::u16string trimmed(std::u16string text) {
    const auto isSpace = [](char16_t ch) { return ch == u' ' || ch == u'\n' || ch == u'\t'; };
    while (!text.empty() && isSpace(text.back()))
        text.pop_back();
    const auto start = std::find_if_not(text.begin(), text.end(), isSpace);
    text.erase(text.begin(), start);
    return text;
}

bool isEscaped(const RichText &body, std::size_t index) {
    return index > 0 && body[index - 1].ch == u'\\';
}

// $...$ or $$...$$ ending at end. Like Pandoc, inline math has no space just inside its
// dollars, so "costs $5 and $6" stays text; blocks may span several lines.
std::optional<EquationSpan> dollarSpan(const RichText &body, std::size_t end) {
    if (!isDollar(body[end - 1]))
        return std::nullopt;
    std::size_t close = end - 1;
    const bool block = close > 0 && isDollar(body[close - 1]);
    if (block)
        --close;
    std::u16string tex;
    for (std::size_t i = close; i-- > 0;) {
        const RichUnit &unit = body[i];
        if (unit.equation || (unit.ch == u'\n' && !block))
            return std::nullopt;
        if (!isDollar(unit)) {
            tex.insert(tex.begin(), unit.ch);
            continue;
        }
        const bool opensBlock = i > 0 && isDollar(body[i - 1]);
        if (isEscaped(body, i) || opensBlock != block)
            return std::nullopt;
        if (!block && (tex.empty() || tex.front() == u' ' || tex.back() == u' '))
            return std::nullopt;
        tex = block ? trimmed(std::move(tex)) : std::move(tex);
        if (tex.empty())
            return std::nullopt;
        return EquationSpan{i - (block ? 1 : 0), std::move(tex), block};
    }
    return std::nullopt;
}

// \[...\] ending at end.
std::optional<EquationSpan> bracketSpan(const RichText &body, std::size_t end) {
    if (end < 4 || body[end - 1].ch != u']' || body[end - 2].ch != u'\\' ||
        isEscaped(body, end - 2))
        return std::nullopt;
    std::u16string tex;
    for (std::size_t i = end - 2; i-- > 0;) {
        if (body[i].equation)
            return std::nullopt;
        if (body[i].ch == u'[' && i > 0 && body[i - 1].ch == u'\\' && !isEscaped(body, i - 1)) {
            tex = trimmed(std::move(tex));
            if (tex.empty())
                return std::nullopt;
            return EquationSpan{i - 1, std::move(tex), true};
        }
        tex.insert(tex.begin(), body[i].ch);
    }
    return std::nullopt;
}

} // namespace

std::size_t placeEquation(RichText &body, std::size_t index, RichUnit equation) {
    const bool block = equation.displayMath;
    const auto lineBreak = [] { return plainRichText(u"\n").front(); };
    if (block && index > 0 && body[index - 1].ch != u'\n')
        body.insert(body.begin() + std::ptrdiff_t(index++), lineBreak());
    body.insert(body.begin() + std::ptrdiff_t(index), std::move(equation));
    if (block && (index + 1 == body.size() || body[index + 1].ch != u'\n'))
        body.insert(body.begin() + std::ptrdiff_t(index + 1), lineBreak());
    return index;
}

std::optional<std::size_t> convertTypedEquation(RichText &body, std::size_t end) {
    if (end == 0 || end > body.size())
        return std::nullopt;
    std::optional<EquationSpan> span = dollarSpan(body, end);
    if (!span)
        span = bracketSpan(body, end);
    if (!span)
        return std::nullopt;
    body.erase(body.begin() + std::ptrdiff_t(span->first), body.begin() + std::ptrdiff_t(end));
    const std::size_t index =
        placeEquation(body, span->first, equationUnit(toUtf8(span->tex), span->block));
    return index + (span->block ? 2 : 1); // a block's cursor goes below it
}

} // namespace peerchalk
