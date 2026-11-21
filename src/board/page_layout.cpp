#include "board/page_layout.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace peerchalk {
namespace {

void checkSize(PaperSize size) {
    if (!std::isfinite(size.width) || !std::isfinite(size.height) || size.width <= 0.0F ||
        size.height <= 0.0F)
        throw std::invalid_argument("page sizes must be finite and positive");
}

float columnWidth(const ColumnSpec &column) {
    checkSize(column.paper);
    float width = column.paper.width;
    for (const ColumnItem &item : column.items) {
        if (item.kind == ColumnItemKind::Page && item.size) {
            checkSize(*item.size);
            width = std::max(width, item.size->width);
        }
    }
    return width;
}

PlacedColumn placeColumn(const ColumnSpec &spec, float left, const LayoutMetrics &metrics) {
    const float width = columnWidth(spec);
    const float right = left + width;
    PlacedColumn column{spec.id, {}, {left, 0.0F, right, metrics.headingHeight},
                        {},      {}, spec.background};
    float y = metrics.headingHeight;
    for (std::size_t i = 0; i < spec.items.size(); ++i) {
        const ColumnItem &item = spec.items[i];
        column.insertSlots.push_back({i, {left, y, right, y + metrics.itemGap}});
        y += metrics.itemGap;
        if (item.kind == ColumnItemKind::Subheading) {
            column.items.push_back(
                {item.id, item.kind, {left, y, right, y + metrics.subheadingHeight}});
            y += metrics.subheadingHeight;
            continue;
        }
        const PaperSize size = item.size.value_or(spec.paper);
        const float x = left + (width - size.width) / 2.0F;
        column.items.push_back(
            {item.id, item.kind, {x, y, x + size.width, y + size.height}, item.pdf});
        y += size.height;
    }
    column.insertSlots.push_back({spec.items.size(), {left, y, right, y + metrics.itemGap}});
    column.bounds = {left, 0.0F, right, y + metrics.itemGap};
    return column;
}

struct NamedPaper {
    const char *name;
    PaperSize size;
};

constexpr NamedPaper namedPapers[] = {
    {"A4", paperA4}, {"A5", paperA5}, {"Letter", paperLetter}, {"Legal", paperLegal}};

bool overlaps(const Bounds &a, const Bounds &b) {
    return a.left <= b.right && b.left <= a.right && a.top <= b.bottom && b.top <= a.bottom;
}

} // namespace

PageLayout layoutPages(const std::vector<ColumnSpec> &columns, const LayoutMetrics &metrics) {
    PageLayout layout;
    float left = 0.0F;
    for (std::size_t i = 0; i < columns.size(); ++i) {
        layout.columnSlots.push_back(
            {i, {left - metrics.columnGap, 0.0F, left, metrics.headingHeight}});
        layout.columns.push_back(placeColumn(columns[i], left, metrics));
        left = layout.columns.back().bounds.right + metrics.columnGap;
    }
    const float end = left - (columns.empty() ? 0.0F : metrics.columnGap);
    layout.columnSlots.push_back(
        {columns.size(), {end, 0.0F, end + metrics.columnGap, metrics.headingHeight}});
    return layout;
}

std::optional<PaperSize> paperNamed(std::string_view name) {
    for (const NamedPaper &paper : namedPapers) {
        if (name == paper.name)
            return paper.size;
    }
    return std::nullopt;
}

std::string paperName(PaperSize size) {
    constexpr float tolerance = 0.5F; // about 0.13 mm
    const auto near = [](float a, float b) { return std::abs(a - b) <= tolerance; };
    for (const NamedPaper &paper : namedPapers) {
        const PaperSize p = paper.size;
        if ((near(size.width, p.width) && near(size.height, p.height)) ||
            (near(size.width, p.height) && near(size.height, p.width)))
            return paper.name;
    }
    return {};
}

// Items are stacked in order, so the first one reaching area is found by binary search.
std::vector<ItemRef> itemsIn(const PageLayout &layout, const Bounds &area) {
    std::vector<ItemRef> found;
    for (std::size_t c = 0; c < layout.columns.size(); ++c) {
        const PlacedColumn &column = layout.columns[c];
        if (!overlaps(column.bounds, area))
            continue;
        const auto first = std::ranges::lower_bound(
            column.items, area.top, {}, [](const PlacedItem &item) { return item.bounds.bottom; });
        for (auto it = first; it != column.items.end() && it->bounds.top <= area.bottom; ++it) {
            if (overlaps(it->bounds, area))
                found.push_back({c, static_cast<std::size_t>(it - column.items.begin())});
        }
    }
    return found;
}

std::optional<ItemRef> pageAt(const PageLayout &layout, Point point) {
    for (const ItemRef ref : itemsIn(layout, {point.x, point.y, point.x, point.y})) {
        const PlacedItem &item = layout.columns[ref.column].items[ref.item];
        if (item.kind == ColumnItemKind::Page && item.bounds.contains(point))
            return ref;
    }
    return std::nullopt;
}

} // namespace peerchalk
