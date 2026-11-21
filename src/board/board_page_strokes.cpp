// The strokes on a page board's pages: one Yrs array per page in "pageStrokes", mirrored
// per page and decoded only when a page's strokes are asked for after a change.
#include "board/board_native.hpp"
#include "board/board_pages.hpp"

#include <algorithm>

namespace peerchalk {
namespace {

bool hasPage(const std::vector<ColumnSpec> &columns, const std::string &pageId) {
    return std::ranges::any_of(columns, [&](const ColumnSpec &column) {
        return std::ranges::any_of(column.items, [&](const ColumnItem &item) {
            return item.kind == ColumnItemKind::Page && item.id == pageId;
        });
    });
}

void markKeys(std::set<std::string> &stale, const YMapEvent *event) {
    std::uint32_t length = 0;
    YEventKeyChange *keys = ymap_event_keys(event, &length);
    for (std::uint32_t i = 0; i < length; ++i)
        stale.insert(keys[i].key);
    yevent_keys_destroy(keys, length);
}

// A change inside a page's array: its path from "pageStrokes" starts with the page id.
void markPath(std::set<std::string> &stale, const YArrayEvent *event) {
    std::uint32_t length = 0;
    YPathSegment *path = yarray_event_path(event, &length);
    if (length > 0 && path[0].tag == Y_EVENT_PATH_KEY)
        stale.insert(path[0].value.key);
    ypath_destroy(path, length);
}

// A page made by this version has its array; one that lacks it gets one now.
const Branch *pageStrokeArray(const Board::Native &native, YTransaction *txn,
                              const std::string &pageId) {
    const Output value(ymap_get(native.pageStrokes, txn, pageId.c_str()), youtput_destroy);
    if (value && value->tag == Y_ARRAY)
        return youtput_read_yarray(value.get());
    const YInput empty = yinput_yarray(nullptr, 0);
    ymap_insert(native.pageStrokes, txn, pageId.c_str(), &empty);
    const Output created(ymap_get(native.pageStrokes, txn, pageId.c_str()), youtput_destroy);
    return youtput_read_yarray(created.get());
}

} // namespace

void BoardPages::strokesChanged(std::uint32_t count, const void *events) {
    const auto *list = static_cast<const YEvent *>(events);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (list[i].tag == Y_MAP)
            markKeys(staleStrokes_, &list[i].content.map);
        else if (list[i].tag == Y_ARRAY)
            markPath(staleStrokes_, &list[i].content.array);
    }
    if (changed_)
        changed_();
}

const std::vector<Stroke> &BoardPages::strokes(const std::string &pageId) const {
    const auto cached = strokes_.find(pageId);
    if (cached != strokes_.end() && !staleStrokes_.contains(pageId))
        return cached->second;
    std::vector<Stroke> decoded;
    {
        const BoardTransaction txn = native_.read();
        const Output value(ymap_get(native_.pageStrokes, txn.get(), pageId.c_str()),
                           youtput_destroy);
        if (value && value->tag == Y_ARRAY) {
            const Branch *array = youtput_read_yarray(value.get());
            for (std::uint32_t i = 0, length = yarray_len(array); i < length; ++i) {
                const Output entry(yarray_get(array, txn.get(), i), youtput_destroy);
                decoded.push_back(entry ? decodeStrokeEntry(*entry) : Stroke(0U, 1.0F));
            }
        }
    }
    staleStrokes_.erase(pageId);
    return strokes_[pageId] = std::move(decoded);
}

void BoardPages::addStroke(const std::string &pageId, const Stroke &stroke) {
    if (!hasPage(columns(), pageId))
        return;
    const Bytes encoded = encodeStroke(stroke);
    {
        const BoardTransaction txn = native_.write(true);
        const Branch *array = pageStrokeArray(native_, txn.get(), pageId);
        const YInput input =
            yinput_binary(reinterpret_cast<const char *>(encoded.data()), checkedLength(encoded));
        yarray_insert_range(array, txn.get(), yarray_len(array), &input, 1);
    }
    yundo_manager_stop(native_.undo.get());
}

std::size_t BoardPages::eraseNear(const std::string &pageId, Point point, float radius) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(radius) ||
        radius < 0.0F)
        throw std::invalid_argument("Eraser position and radius must be finite and non-negative");
    const std::vector<Stroke> &list = strokes(pageId);
    std::vector<std::uint32_t> hits;
    for (std::size_t i = list.size(); i-- > 0;) {
        if (strokeNear(list[i], point, radius))
            hits.push_back(static_cast<std::uint32_t>(i));
    }
    if (!hits.empty()) {
        const BoardTransaction txn = native_.write(true);
        const Branch *array = pageStrokeArray(native_, txn.get(), pageId);
        for (const std::uint32_t index : hits) // descending, so earlier indices stay valid
            yarray_remove_range(array, txn.get(), index, 1);
    }
    return hits.size();
}

} // namespace peerchalk
