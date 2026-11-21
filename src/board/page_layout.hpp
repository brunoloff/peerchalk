#pragma once

#include "board/geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace peerchalk {

// A sheet's size in board units (1/96 inch), so that exports keep real paper sizes.
struct PaperSize {
    float width;
    float height;
};

inline constexpr float unitsPerMillimetre = 96.0F / 25.4F;
inline constexpr PaperSize paperA4{210.0F * unitsPerMillimetre, 297.0F * unitsPerMillimetre};
inline constexpr PaperSize paperA5{148.0F * unitsPerMillimetre, 210.0F * unitsPerMillimetre};
inline constexpr PaperSize paperLetter{816.0F, 1056.0F}; // 8.5 × 11 in
inline constexpr PaperSize paperLegal{816.0F, 1344.0F};  // 8.5 × 14 in

[[nodiscard]] inline PaperSize landscape(PaperSize paper) { return {paper.height, paper.width}; }

// The paper sizes offered by name ("A4", "A5", "Letter", "Legal"), portrait.
[[nodiscard]] std::optional<PaperSize> paperNamed(std::string_view name);
// The name of a size that is one of those, either way up; "" for any other size.
[[nodiscard]] std::string paperName(PaperSize size);

// What is printed on a column's pages under the ink.
enum class PageBackground : std::uint8_t { Blank, Ruled, Squared, Dotted };

// What a column holds, top to bottom: pages and subheadings (h2).
enum class ColumnItemKind : std::uint8_t { Page, Subheading };

// A page of a PDF shown under a page's ink. The PDF is a blob found by its SHA-256 (hex).
struct PdfSource {
    std::string blob;
    int page{0};

    bool operator==(const PdfSource &) const = default;
};

struct ColumnItem {
    std::string id;
    ColumnItemKind kind{ColumnItemKind::Page};
    std::optional<PaperSize> size;  // pages only; none: the column's paper
    std::string text{};             // subheadings only
    std::optional<PdfSource> pdf{}; // pages showing a PDF page; they keep their own size
};

// A column of a page board: its paper sets its width and the size of its pages.
struct ColumnSpec {
    std::string id;
    PaperSize paper{paperA4};
    std::vector<ColumnItem> items;
    std::string title{}; // the column's heading (h1)
    PageBackground background{PageBackground::Blank};
};

// Spacing of a page board, in board units.
struct LayoutMetrics {
    float columnGap{96.0F};
    float headingHeight{120.0F}; // the band above each column holding its heading (h1)
    float itemGap{32.0F};
    float subheadingHeight{72.0F};
};

struct PlacedItem {
    std::string id;
    ColumnItemKind kind;
    Bounds bounds;
    std::optional<PdfSource> pdf{};
};

// Where something can be added: an item before index `index` of a column, or a column
// before index `index` of the board.
struct InsertSlot {
    std::size_t index;
    Bounds bounds;
};

struct PlacedColumn {
    std::string id;
    Bounds bounds;  // from the top of the heading band to the last slot
    Bounds heading; // the heading band
    std::vector<PlacedItem> items;
    std::vector<InsertSlot> insertSlots; // items.size() + 1, before each item and after the last
    PageBackground background{PageBackground::Blank};
};

struct PageLayout {
    std::vector<PlacedColumn> columns;
    std::vector<InsertSlot> columnSlots; // columns.size() + 1, in the gaps around columns
};

// Which item of which column.
struct ItemRef {
    std::size_t column;
    std::size_t item;

    bool operator==(const ItemRef &) const = default;
};

// Places columns left to right from x = 0, each a heading band and then its items with
// itemGap around them; pages narrower than their column are centred. Throws
// std::invalid_argument for a paper or page size that is not finite and positive.
[[nodiscard]] PageLayout layoutPages(const std::vector<ColumnSpec> &columns,
                                     const LayoutMetrics &metrics = {});
// The items that overlap area, column by column, top to bottom.
[[nodiscard]] std::vector<ItemRef> itemsIn(const PageLayout &layout, const Bounds &area);
// The page under point, if any (subheadings are not pages).
[[nodiscard]] std::optional<ItemRef> pageAt(const PageLayout &layout, Point point);

} // namespace peerchalk
