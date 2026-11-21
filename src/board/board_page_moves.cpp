// Moving a page board's items and columns (see BoardPages::moveItem).
#include "board/board_pages_native.hpp"

namespace peerchalk {

using namespace page_yrs;

void BoardPages::moveItem(const std::string &fromColumn, const std::string &itemId,
                          const std::string &toColumn, const std::string &before) {
    if (itemId == before)
        return;
    yundo_manager_stop(native_.undo.get());
    [&] {
        const BoardTransaction txn = native_.write(true);
        const Branch *from = columnItems(native_, txn.get(), fromColumn);
        const Branch *to = columnItems(native_, txn.get(), toColumn);
        const auto index = from && to ? indexOf(from, txn.get(), itemId) : std::nullopt;
        const auto item =
            index ? readItem(mapAt(from, txn.get(), *index), txn.get()) : std::nullopt;
        if (!item)
            return;
        yarray_remove_range(from, txn.get(), *index, 1);
        const std::uint32_t at = indexOf(to, txn.get(), before).value_or(yarray_len(to));
        MapInput map;
        writeItem(map, *item);
        const YInput input = map.build();
        yarray_insert_range(to, txn.get(), at, &input, 1);
    }();
    yundo_manager_stop(native_.undo.get());
}

void BoardPages::moveColumn(const std::string &columnId, const std::string &before) {
    if (columnId == before)
        return;
    yundo_manager_stop(native_.undo.get());
    [&] {
        const BoardTransaction txn = native_.write(true);
        const auto index = indexOf(native_.columns, txn.get(), columnId);
        const auto column =
            index ? readColumn(mapAt(native_.columns, txn.get(), *index), txn.get()) : std::nullopt;
        if (!column)
            return;
        yarray_remove_range(native_.columns, txn.get(), *index, 1);
        const std::uint32_t at =
            indexOf(native_.columns, txn.get(), before).value_or(yarray_len(native_.columns));
        insertColumnSpec(native_, txn.get(), at, *column);
    }();
    yundo_manager_stop(native_.undo.get());
}

} // namespace peerchalk
