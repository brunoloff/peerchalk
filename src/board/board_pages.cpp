// The structure of a page board: its kind, its columns and their items (see board_pages.hpp).
#include "board/board_pages.hpp"

#include "board/board_pages_native.hpp"

#include <exception>

namespace peerchalk {
namespace {

using namespace page_yrs;

constexpr char layoutName[] = "layout";
constexpr char columnsName[] = "columns";
constexpr char pageStrokesName[] = "pageStrokes";
constexpr char observerKey[] = "peerchalk-pages";

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
    ++columnsVersion_;
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
            removeColumnContent(native_, txn.get(), items);
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

void BoardPages::setColumnPaper(const std::string &columnId, PaperSize paper, bool allPages) {
    checkPaper(paper);
    yundo_manager_stop(native_.undo.get());
    {
        const BoardTransaction txn = native_.write(true);
        if (const std::optional<std::uint32_t> index =
                indexOf(native_.columns, txn.get(), columnId)) {
            const Branch *column = mapAt(native_.columns, txn.get(), *index);
            const std::optional<ColumnSpec> old = readColumn(column, txn.get());
            if (const Branch *items = old ? itemsOf(column, txn.get()) : nullptr)
                setPageSizes(items, txn.get(), allPages ? std::nullopt : std::optional(old->paper));
            writeSize(column, txn.get(), "paperWidth", "paperHeight", paper);
        }
    }
    yundo_manager_stop(native_.undo.get());
}

void BoardPages::setPageSize(const std::string &columnId, const std::string &pageId,
                             std::optional<PaperSize> size) {
    if (size)
        checkPaper(*size);
    yundo_manager_stop(native_.undo.get());
    {
        const BoardTransaction txn = native_.write(true);
        const Branch *items = columnItems(native_, txn.get(), columnId);
        const auto index = items ? indexOf(items, txn.get(), pageId) : std::nullopt;
        const Branch *page = index ? mapAt(items, txn.get(), *index) : nullptr;
        if (page != nullptr && mapString(page, txn.get(), "kind") == "page")
            setOwnSize(page, txn.get(), size);
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
            writeItem(map, {id, kind, std::nullopt, text});
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
            removePageContent(native_, txn.get(), itemId);
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
