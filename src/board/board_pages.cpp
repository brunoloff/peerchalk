// The structure of a page board: its kind, its columns and their items (see board_pages.hpp).
#include "board/board_pages.hpp"

#include "board/board_native.hpp"

#include <cinttypes>
#include <cstdio>
#include <deque>
#include <exception>
#include <random>

namespace peerchalk {
namespace {

constexpr char layoutName[] = "layout";
constexpr char columnsName[] = "columns";
constexpr char pageStrokesName[] = "pageStrokes";
constexpr char observerKey[] = "peerchalk-pages";
constexpr char pagesKind[] = "pages";
constexpr double maxPageSide = 100000.0; // about 26 m; anything larger is not paper

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

// A Yrs map to insert; the keys and strings it points to live as long as it does.
class MapInput {
  public:
    void add(const char *key, YInput value) {
        keys_.push_back(const_cast<char *>(key)); // Yrs only reads the keys
        values_.push_back(value);
    }
    void addString(const char *key, const std::string &value) {
        add(key, yinput_string(strings_.emplace_back(value).c_str()));
    }
    [[nodiscard]] YInput build() {
        return yinput_ymap(keys_.data(), values_.data(), static_cast<std::uint32_t>(keys_.size()));
    }

  private:
    std::vector<char *> keys_;
    std::vector<YInput> values_;
    std::deque<std::string> strings_;
};

void describeItem(MapInput &map, const std::string &id, ColumnItemKind kind,
                  const std::string &text) {
    map.addString("id", id);
    map.addString("kind", kind == ColumnItemKind::Page ? "page" : "subheading");
    if (kind == ColumnItemKind::Subheading)
        map.addString("text", text);
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

std::optional<PaperSize> readSize(const Branch *map, const YTransaction *txn, const char *width,
                                  const char *height) {
    const auto w = mapNumber(map, txn, width);
    const auto h = mapNumber(map, txn, height);
    if (!w || !h || *w <= 0.0 || *h <= 0.0 || *w > maxPageSide || *h > maxPageSide)
        return std::nullopt;
    return PaperSize{static_cast<float>(*w), static_cast<float>(*h)};
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
    ColumnSpec column{
        mapString(map, txn, "id"), paper.value_or(paperA4), {}, mapString(map, txn, "title")};
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

std::string insertColumn(const Board::Native &native, YTransaction *txn, std::uint32_t index,
                         PaperSize paper, const std::string &title) {
    const std::string columnId = newId();
    const std::string pageId = newId();
    MapInput page;
    describeItem(page, pageId, ColumnItemKind::Page, {});
    YInput pageInput = page.build();
    MapInput column;
    column.addString("id", columnId);
    column.addString("title", title);
    column.add("paperWidth", yinput_float(paper.width));
    column.add("paperHeight", yinput_float(paper.height));
    column.add("items", yinput_yarray(&pageInput, 1));
    const YInput input = column.build();
    yarray_insert_range(native.columns, txn, index, &input, 1);
    createStrokeArray(native, txn, pageId);
    return columnId;
}

const Branch *columnItems(const Board::Native &native, const YTransaction *txn,
                          const std::string &columnId) {
    const std::optional<std::uint32_t> index = indexOf(native.columns, txn, columnId);
    return index ? itemsOf(mapAt(native.columns, txn, *index), txn) : nullptr;
}

void removePageStrokes(const Board::Native &native, YTransaction *txn, const Branch *items) {
    for (std::uint32_t i = 0, length = yarray_len(items); i < length; ++i) {
        if (const Branch *item = mapAt(items, txn, i))
            ymap_remove(native.pageStrokes, txn, mapString(item, txn, "id").c_str());
    }
}

template <typename Callback> void guarded(Callback callback) {
    // Exceptions must not unwind through the Rust library.
    try {
        callback();
    } catch (...) {
        std::terminate();
    }
}

} // namespace

BoardPages::BoardPages(Board::Native &native, std::function<void()> changed)
    : native_(native), changed_(std::move(changed)) {
    YDoc *doc = native_.doc.get();
    if (!(native_.layout = ymap(doc, layoutName)) ||
        !(native_.columns = yarray(doc, columnsName)) ||
        !(native_.pageStrokes = ymap(doc, pageStrokesName)))
        throw std::runtime_error("Yrs page board could not be created");
    yundo_manager_add_scope(native_.undo.get(), doc, native_.columns);
    yundo_manager_add_scope(native_.undo.get(), doc, native_.pageStrokes);
    yobserve_deep(native_.columns, sizeof(observerKey) - 1, observerKey, this,
                  [](void *state, std::uint32_t, const YEvent *) {
                      guarded([state] { static_cast<BoardPages *>(state)->columnsChanged(); });
                  });
    yobserve_deep(native_.pageStrokes, sizeof(observerKey) - 1, observerKey, this,
                  [](void *state, std::uint32_t count, const YEvent *events) {
                      guarded(
                          [&] { static_cast<BoardPages *>(state)->strokesChanged(count, events); });
                  });
}

BoardPages::~BoardPages() {
    yunobserve_deep(native_.columns, sizeof(observerKey) - 1, observerKey);
    yunobserve_deep(native_.pageStrokes, sizeof(observerKey) - 1, observerKey);
}

void BoardPages::columnsChanged() {
    columnsStale_ = true;
    if (changed_)
        changed_();
}

BoardKind BoardPages::kind() const {
    const BoardTransaction txn = native_.read();
    return mapString(native_.layout, txn.get(), "kind") == pagesKind ? BoardKind::Pages
                                                                     : BoardKind::Infinite;
}

void BoardPages::start(PaperSize paper, const std::string &title) {
    checkPaper(paper);
    {
        const BoardTransaction txn = native_.read();
        if (yarray_len(native_.strokes) > 0 || ymap_len(native_.textBoxes, txn.get()) > 0 ||
            yarray_len(native_.columns) > 0 ||
            mapString(native_.layout, txn.get(), "kind") == pagesKind)
            throw std::logic_error("only an empty board can become a page board");
    }
    const BoardTransaction txn = native_.write(false);
    const YInput kind = yinput_string(pagesKind);
    ymap_insert(native_.layout, txn.get(), "kind", &kind);
    insertColumn(native_, txn.get(), 0, paper, title);
}

const std::vector<ColumnSpec> &BoardPages::columns() const {
    if (!columnsStale_)
        return columns_;
    const BoardTransaction txn = native_.read();
    columns_.clear();
    for (std::uint32_t i = 0, length = yarray_len(native_.columns); i < length; ++i) {
        const Branch *map = mapAt(native_.columns, txn.get(), i);
        if (std::optional<ColumnSpec> column = map ? readColumn(map, txn.get()) : std::nullopt)
            columns_.push_back(*std::move(column));
    }
    columnsStale_ = false;
    return columns_;
}

std::string BoardPages::addColumn(const std::string &before, PaperSize paper,
                                  const std::string &title) {
    checkPaper(paper);
    yundo_manager_stop(native_.undo.get());
    std::string id;
    {
        const BoardTransaction txn = native_.write(true);
        const std::uint32_t index =
            indexOf(native_.columns, txn.get(), before).value_or(yarray_len(native_.columns));
        id = insertColumn(native_, txn.get(), index, paper, title);
    }
    yundo_manager_stop(native_.undo.get());
    return id;
}

void BoardPages::removeColumn(const std::string &columnId) {
    yundo_manager_stop(native_.undo.get());
    {
        const BoardTransaction txn = native_.write(true);
        const std::optional<std::uint32_t> index = indexOf(native_.columns, txn.get(), columnId);
        if (!index)
            return;
        if (const Branch *items = itemsOf(mapAt(native_.columns, txn.get(), *index), txn.get()))
            removePageStrokes(native_, txn.get(), items);
        yarray_remove_range(native_.columns, txn.get(), *index, 1);
    }
    yundo_manager_stop(native_.undo.get());
}

void BoardPages::setColumnTitle(const std::string &columnId, const std::string &title) {
    yundo_manager_stop(native_.undo.get());
    {
        const BoardTransaction txn = native_.write(true);
        if (const std::optional<std::uint32_t> index =
                indexOf(native_.columns, txn.get(), columnId)) {
            const YInput value = yinput_string(title.c_str());
            ymap_insert(mapAt(native_.columns, txn.get(), *index), txn.get(), "title", &value);
        }
    }
    yundo_manager_stop(native_.undo.get());
}

void BoardPages::setColumnPaper(const std::string &columnId, PaperSize paper) {
    checkPaper(paper);
    yundo_manager_stop(native_.undo.get());
    {
        const BoardTransaction txn = native_.write(true);
        if (const std::optional<std::uint32_t> index =
                indexOf(native_.columns, txn.get(), columnId)) {
            const Branch *column = mapAt(native_.columns, txn.get(), *index);
            const YInput width = yinput_float(paper.width);
            const YInput height = yinput_float(paper.height);
            ymap_insert(column, txn.get(), "paperWidth", &width);
            ymap_insert(column, txn.get(), "paperHeight", &height);
        }
    }
    yundo_manager_stop(native_.undo.get());
}

std::string BoardPages::insertItem(const std::string &columnId, const std::string &before,
                                   ColumnItemKind kind, const std::string &text) {
    yundo_manager_stop(native_.undo.get());
    std::string id;
    {
        const BoardTransaction txn = native_.write(true);
        if (const Branch *items = columnItems(native_, txn.get(), columnId)) {
            const std::uint32_t index =
                indexOf(items, txn.get(), before).value_or(yarray_len(items));
            id = newId();
            MapInput map;
            describeItem(map, id, kind, text);
            const YInput input = map.build();
            yarray_insert_range(items, txn.get(), index, &input, 1);
            if (kind == ColumnItemKind::Page)
                createStrokeArray(native_, txn.get(), id);
        }
    }
    yundo_manager_stop(native_.undo.get());
    return id;
}

void BoardPages::removeItem(const std::string &columnId, const std::string &itemId) {
    yundo_manager_stop(native_.undo.get());
    {
        const BoardTransaction txn = native_.write(true);
        const Branch *items = columnItems(native_, txn.get(), columnId);
        if (const auto index = items ? indexOf(items, txn.get(), itemId) : std::nullopt) {
            ymap_remove(native_.pageStrokes, txn.get(), itemId.c_str());
            yarray_remove_range(items, txn.get(), *index, 1);
        }
    }
    yundo_manager_stop(native_.undo.get());
}

void BoardPages::setSubheadingText(const std::string &columnId, const std::string &itemId,
                                   const std::string &text) {
    yundo_manager_stop(native_.undo.get());
    {
        const BoardTransaction txn = native_.write(true);
        const Branch *items = columnItems(native_, txn.get(), columnId);
        if (const auto index = items ? indexOf(items, txn.get(), itemId) : std::nullopt) {
            const YInput value = yinput_string(text.c_str());
            ymap_insert(mapAt(items, txn.get(), *index), txn.get(), "text", &value);
        }
    }
    yundo_manager_stop(native_.undo.get());
}

} // namespace peerchalk
