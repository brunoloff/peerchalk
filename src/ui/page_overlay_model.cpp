#include "ui/page_overlay_model.hpp"

#include "board/page_map.hpp"

#include <algorithm>

namespace peerchalk {
namespace {

bool overlaps(const Bounds &a, const Bounds &b) {
    return a.left <= b.right && b.left <= a.right && a.top <= b.bottom && b.top <= a.bottom;
}

const char *kindName(PageOverlayModel::Kind kind) {
    switch (kind) {
    case PageOverlayModel::Kind::Heading:
        return "heading";
    case PageOverlayModel::Kind::Subheading:
        return "subheading";
    case PageOverlayModel::Kind::Gap:
        return "gap";
    case PageOverlayModel::Kind::PageNumber:
        return "page";
    }
    return "";
}

using Row = PageOverlayModel::Row;
using Kind = PageOverlayModel::Kind;

// One column's rows in area.
void addColumn(std::vector<Row> &rows, const PlacedColumn &placed, const ColumnSpec &spec,
               const Bounds &area, const std::function<bool(const std::string &)> &missing) {
    if (overlaps(placed.heading, area))
        rows.push_back({"h" + spec.id, Kind::Heading, spec.id, {}, spec.title, 0, placed.heading});
    int number = 0;
    for (std::size_t i = 0; i < placed.items.size(); ++i) {
        const PlacedItem &item = placed.items[i];
        number += item.kind == ColumnItemKind::Page ? 1 : 0;
        if (!overlaps(item.bounds, area))
            continue;
        if (item.kind == ColumnItemKind::Page)
            rows.push_back({"p" + item.id,
                            Kind::PageNumber,
                            spec.id,
                            item.id,
                            {},
                            number,
                            item.bounds,
                            item.pdf && missing && missing(item.pdf->blob),
                            item.pdf ? item.pdf->blob : std::string()});
        else
            rows.push_back({"s" + item.id, Kind::Subheading, spec.id, item.id,
                            i < spec.items.size() ? spec.items[i].text : std::string(), 0,
                            item.bounds});
    }
    for (const InsertSlot &slot : placed.insertSlots) {
        if (!overlaps(slot.bounds, area))
            continue;
        const std::string before =
            slot.index < placed.items.size() ? placed.items[slot.index].id : std::string();
        rows.push_back(
            {"g" + spec.id + ":" + before, Kind::Gap, spec.id, before, {}, 0, slot.bounds});
    }
}

} // namespace

std::vector<Row> pageOverlayRows(const PageMap &map, const std::vector<ColumnSpec> &columns,
                                 const Bounds &area,
                                 const std::function<bool(const std::string &)> &missing) {
    std::vector<Row> rows;
    const PageLayout &layout = map.layout();
    for (std::size_t c = 0; c < layout.columns.size() && c < columns.size(); ++c) {
        if (overlaps(layout.columns[c].bounds, area))
            addColumn(rows, layout.columns[c], columns[c], area, missing);
    }
    std::ranges::sort(rows, {}, &Row::key);
    return rows;
}

int PageOverlayModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant PageOverlayModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const Row &row = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case KindRole:
        return QString::fromLatin1(kindName(row.kind));
    case Column:
        return QString::fromStdString(row.column);
    case Item:
        return QString::fromStdString(row.item);
    case Text:
        return QString::fromStdString(row.text);
    case Number:
        return row.number;
    case X:
        return row.bounds.left;
    case Y:
        return row.bounds.top;
    case Width:
        return row.bounds.width();
    case Height:
        return row.bounds.height();
    case Waiting:
        return row.waiting;
    case Blob:
        return QString::fromStdString(row.blob);
    default:
        return {};
    }
}

QHash<int, QByteArray> PageOverlayModel::roleNames() const {
    return {{KindRole, "kind"},     {Column, "columnId"}, {Item, "itemId"}, {Text, "label"},
            {Number, "number"},     {X, "itemX"},         {Y, "itemY"},     {Width, "itemWidth"},
            {Height, "itemHeight"}, {Waiting, "waiting"}, {Blob, "blob"}};
}

void PageOverlayModel::sync(const std::vector<Row> &rows) {
    std::size_t next = 0;
    for (const Row &row : rows) {
        while (next < rows_.size() && rows_[next].key < row.key) {
            beginRemoveRows({}, int(next), int(next));
            rows_.erase(rows_.begin() + std::ptrdiff_t(next));
            endRemoveRows();
        }
        if (next == rows_.size() || rows_[next].key != row.key) {
            beginInsertRows({}, int(next), int(next));
            rows_.insert(rows_.begin() + std::ptrdiff_t(next), row);
            endInsertRows();
        } else if (!(rows_[next] == row)) {
            rows_[next] = row;
            emit dataChanged(index(int(next)), index(int(next)));
        }
        ++next;
    }
    if (next < rows_.size()) {
        beginRemoveRows({}, int(next), int(rows_.size()) - 1);
        rows_.resize(next);
        endRemoveRows();
    }
}

} // namespace peerchalk
