// The current page of a page board on BoardCanvas: the one in the middle of the view, which
// the side panel describes and, while it does, outlines in blue.
#include "ui/board_canvas.hpp"

#include "board/board_pages.hpp"
#include "ui/conversions.hpp"
#include "ui/scene_nodes.hpp"

#include <QSGGeometryNode>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace peerchalk {
namespace {

// The page of the column under x nearest to y, as (column, item).
std::optional<ItemRef> nearestPage(const PageLayout &layout, Point point) {
    for (std::size_t c = 0; c < layout.columns.size(); ++c) {
        const PlacedColumn &column = layout.columns[c];
        if (point.x < column.bounds.left || point.x > column.bounds.right)
            continue;
        std::optional<ItemRef> best;
        float distance = INFINITY;
        for (std::size_t i = 0; i < column.items.size(); ++i) {
            const Bounds &b = column.items[i].bounds;
            const float d = point.y < b.top ? b.top - point.y : std::max(0.0F, point.y - b.bottom);
            if (column.items[i].kind == ColumnItemKind::Page && d < distance) {
                distance = d;
                best = ItemRef{c, i};
            }
        }
        return best;
    }
    return std::nullopt;
}

} // namespace

void BoardCanvas::updateCurrentPage() {
    QString id;
    if (pageBoard_) {
        const QPointF centre = viewCenter();
        if (const std::optional<ItemRef> ref = nearestPage(pageMap_.layout(), toPoint(centre)))
            id = QString::fromStdString(pageMap_.layout().columns[ref->column].items[ref->item].id);
    }
    if (id == currentPageId_)
        return;
    currentPageId_ = id;
    emit currentPageChanged();
    if (showCurrentPage_)
        update();
}

QVariantMap BoardCanvas::currentPage() const {
    if (currentPageId_.isEmpty() || !pageBoard_)
        return {};
    const std::string id = currentPageId_.toStdString();
    for (const ColumnSpec &column : board().pages().columns()) {
        int number = 0;
        int count = 0;
        const ColumnItem *found = nullptr;
        for (const ColumnItem &item : column.items) {
            if (item.kind != ColumnItemKind::Page)
                continue;
            ++count;
            if (item.id == id) {
                found = &item;
                number = count;
            }
        }
        if (found == nullptr)
            continue;
        const PaperSize size = found->size.value_or(column.paper);
        return {{"pageId", currentPageId_},
                {"columnId", QString::fromStdString(column.id)},
                {"columnTitle", QString::fromStdString(column.title)},
                {"number", number},
                {"count", count},
                {"width", size.width},
                {"height", size.height},
                {"pdfPage", found->pdf ? found->pdf->page + 1 : 0}};
    }
    return {};
}

void BoardCanvas::setShowCurrentPage(bool show) {
    if (show == showCurrentPage_)
        return;
    showCurrentPage_ = show;
    emit showCurrentPageChanged();
    update();
}

void BoardCanvas::appendCurrentPageOutline(QSGNode *parent) const {
    if (!showCurrentPage_ || currentPageId_.isEmpty())
        return;
    const std::optional<PlacedPage> page = pageMap_.page(currentPageId_.toStdString());
    if (!page)
        return;
    const float px = 1.0F / viewport_.scale();
    const Bounds &b = page->bounds;
    parent->appendChildNode(createOutlineNode(
        {b.left - 5.0F * px, b.top - 5.0F * px, b.right + 5.0F * px, b.bottom + 5.0F * px},
        2.0F * px, QColor(0x2F, 0x5B, 0xEA)));
}

} // namespace peerchalk
