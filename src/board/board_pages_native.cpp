// Reading and writing the Yrs entries of a page board, shared by its source files.
#include "board/board_pages_native.hpp"

#include <cinttypes>
#include <cstdio>
#include <iterator>
#include <random>

namespace peerchalk::page_yrs {
namespace {

std::optional<PaperSize> readSize(const Branch *map, const YTransaction *txn, const char *width,
                                  const char *height) {
    const auto w = mapNumber(map, txn, width);
    const auto h = mapNumber(map, txn, height);
    if (!w || !h || *w <= 0.0 || *h <= 0.0 || *w > maxPageSide || *h > maxPageSide)
        return std::nullopt;
    return PaperSize{static_cast<float>(*w), static_cast<float>(*h)};
}

constexpr const char *backgroundNames[] = {"blank", "ruled", "squared", "dotted"};

// A background this version does not know is shown blank.
PageBackground readBackground(const Branch *map, const YTransaction *txn) {
    const std::string name = mapString(map, txn, "background");
    for (std::size_t i = 0; i < std::size(backgroundNames); ++i) {
        if (name == backgroundNames[i])
            return static_cast<PageBackground>(i);
    }
    return PageBackground::Blank;
}

} // namespace

const char *backgroundName(PageBackground background) {
    return backgroundNames[static_cast<std::size_t>(background)];
}

std::string newId() {
    thread_local std::mt19937_64 generator{std::random_device{}()};
    char text[17];
    std::snprintf(text, sizeof text, "%016" PRIx64, generator());
    return text;
}

void checkPaper(PaperSize paper) {
    if (!std::isfinite(paper.width) || !std::isfinite(paper.height) || paper.width <= 0.0F ||
        paper.height <= 0.0F || paper.width > maxPageSide || paper.height > maxPageSide)
        throw std::invalid_argument("paper sizes must be finite, positive and below 26 m");
}

void writeItem(MapInput &map, const ColumnItem &item) {
    map.addString("id", item.id);
    map.addString("kind", item.kind == ColumnItemKind::Page ? "page" : "subheading");
    if (item.kind == ColumnItemKind::Subheading)
        map.addString("text", item.text);
    if (item.kind == ColumnItemKind::Page && item.size) {
        map.add("width", yinput_float(item.size->width));
        map.add("height", yinput_float(item.size->height));
    }
}

void writeSize(const Branch *map, YTransaction *txn, const char *width, const char *height,
               PaperSize size) {
    const YInput w = yinput_float(size.width);
    const YInput h = yinput_float(size.height);
    ymap_insert(map, txn, width, &w);
    ymap_insert(map, txn, height, &h);
}

void setOwnSize(const Branch *page, YTransaction *txn, std::optional<PaperSize> size) {
    if (size) {
        writeSize(page, txn, "width", "height", *size);
        return;
    }
    ymap_remove(page, txn, "width");
    ymap_remove(page, txn, "height");
}

void setPageSizes(const Branch *items, YTransaction *txn, std::optional<PaperSize> keep) {
    for (std::uint32_t i = 0, length = yarray_len(items); i < length; ++i) {
        const Branch *item = mapAt(items, txn, i);
        if (item == nullptr || mapString(item, txn, "kind") != "page")
            continue;
        if (!keep)
            setOwnSize(item, txn, std::nullopt);
        else if (!readSize(item, txn, "width", "height"))
            setOwnSize(item, txn, keep);
    }
}

const Branch *mapAt(const Branch *array, const YTransaction *txn, std::uint32_t index) {
    const Output value(yarray_get(array, txn, index), youtput_destroy);
    return value && value->tag == Y_MAP ? youtput_read_ymap(value.get()) : nullptr;
}

std::optional<std::uint32_t> indexOf(const Branch *array, const YTransaction *txn,
                                     const std::string &id) {
    for (std::uint32_t i = 0, length = yarray_len(array); i < length; ++i) {
        const Branch *map = mapAt(array, txn, i);
        if (map != nullptr && mapString(map, txn, "id") == id)
            return i;
    }
    return std::nullopt;
}

const Branch *itemsOf(const Branch *column, const YTransaction *txn) {
    const Output value(ymap_get(column, txn, "items"), youtput_destroy);
    return value && value->tag == Y_ARRAY ? youtput_read_yarray(value.get()) : nullptr;
}

std::optional<ColumnItem> readItem(const Branch *map, const YTransaction *txn) {
    ColumnItem item{mapString(map, txn, "id"), ColumnItemKind::Page, std::nullopt, {}};
    const std::string kind = mapString(map, txn, "kind");
    if (item.id.empty())
        return std::nullopt;
    if (kind == "page") {
        item.size = readSize(map, txn, "width", "height");
    } else if (kind == "subheading") {
        item.kind = ColumnItemKind::Subheading;
        item.text = mapString(map, txn, "text");
    } else {
        return std::nullopt; // a kind of item from a newer version
    }
    return item;
}

std::optional<ColumnSpec> readColumn(const Branch *map, const YTransaction *txn) {
    const std::optional<PaperSize> paper = readSize(map, txn, "paperWidth", "paperHeight");
    ColumnSpec column{mapString(map, txn, "id"),
                      paper.value_or(paperA4),
                      {},
                      mapString(map, txn, "title"),
                      readBackground(map, txn)};
    const Branch *items = itemsOf(map, txn);
    if (column.id.empty() || !paper || items == nullptr)
        return std::nullopt;
    for (std::uint32_t i = 0, length = yarray_len(items); i < length; ++i) {
        const Branch *item = mapAt(items, txn, i);
        if (std::optional<ColumnItem> read = item ? readItem(item, txn) : std::nullopt)
            column.items.push_back(*std::move(read));
    }
    return column;
}

void createStrokeArray(const Board::Native &native, YTransaction *txn, const std::string &page) {
    const YInput empty = yinput_yarray(nullptr, 0);
    ymap_insert(native.pageStrokes, txn, page.c_str(), &empty);
}

void insertColumnSpec(const Board::Native &native, YTransaction *txn, std::uint32_t index,
                      const ColumnSpec &spec) {
    std::deque<MapInput> itemMaps; // each built input points into its map
    std::vector<YInput> items;
    for (const ColumnItem &item : spec.items) {
        writeItem(itemMaps.emplace_back(), item);
        items.push_back(itemMaps.back().build());
    }
    MapInput column;
    column.addString("id", spec.id);
    column.addString("title", spec.title);
    column.add("paperWidth", yinput_float(spec.paper.width));
    column.add("paperHeight", yinput_float(spec.paper.height));
    column.add("background", yinput_string(backgroundName(spec.background)));
    column.add("items", yinput_yarray(items.data(), static_cast<std::uint32_t>(items.size())));
    const YInput input = column.build();
    yarray_insert_range(native.columns, txn, index, &input, 1);
}

std::string insertColumn(const Board::Native &native, YTransaction *txn, std::uint32_t index,
                         const ColumnSpec &like) {
    checkPaper(like.paper);
    const ColumnItem page{newId(), ColumnItemKind::Page, std::nullopt, {}};
    const ColumnSpec spec{newId(), like.paper, {page}, like.title, like.background};
    insertColumnSpec(native, txn, index, spec);
    createStrokeArray(native, txn, page.id);
    return spec.id;
}

const Branch *columnItems(const Board::Native &native, const YTransaction *txn,
                          const std::string &columnId) {
    const std::optional<std::uint32_t> index = indexOf(native.columns, txn, columnId);
    return index ? itemsOf(mapAt(native.columns, txn, *index), txn) : nullptr;
}

void removePageContent(const Board::Native &native, YTransaction *txn, const std::string &pageId) {
    ymap_remove(native.pageStrokes, txn, pageId.c_str());
    std::vector<std::string> boxes;
    {
        const std::unique_ptr<YMapIter, decltype(&ymap_iter_destroy)> entries(
            ymap_iter(native.textBoxes, txn), ymap_iter_destroy);
        while (YMapEntry *raw = ymap_iter_next(entries.get())) {
            const std::unique_ptr<YMapEntry, decltype(&ymap_entry_destroy)> entry(
                raw, ymap_entry_destroy);
            if (entry->value->tag == Y_MAP &&
                mapString(youtput_read_ymap(entry->value), txn, "page") == pageId)
                boxes.emplace_back(entry->key);
        }
    }
    for (const std::string &box : boxes)
        ymap_remove(native.textBoxes, txn, box.c_str());
}

void removeColumnContent(const Board::Native &native, YTransaction *txn, const Branch *items) {
    for (std::uint32_t i = 0, length = yarray_len(items); i < length; ++i) {
        if (const Branch *item = mapAt(items, txn, i))
            removePageContent(native, txn, mapString(item, txn, "id"));
    }
}

} // namespace peerchalk::page_yrs
