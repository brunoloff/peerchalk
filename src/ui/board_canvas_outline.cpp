// The outline of a page board on BoardCanvas: its columns and their subheadings.
#include "ui/board_canvas.hpp"

#include "board/board_pages.hpp"

#include <QVariantMap>

#include <algorithm>

namespace peerchalk {

QVariantList BoardCanvas::pageOutline() const {
    QVariantList outline;
    if (!pageBoard_)
        return outline;
    for (const ColumnSpec &column : board().pages().columns()) {
        const QString columnId = QString::fromStdString(column.id);
        outline.append(QVariantMap{
            {"columnId", columnId},
            {"itemId", QString()},
            {"text", QString::fromStdString(column.title)},
            {"level", 1},
            {"pages",
             int(std::ranges::count(column.items, ColumnItemKind::Page, &ColumnItem::kind))}});
        for (const ColumnItem &item : column.items) {
            if (item.kind == ColumnItemKind::Subheading)
                outline.append(QVariantMap{{"columnId", columnId},
                                           {"itemId", QString::fromStdString(item.id)},
                                           {"text", QString::fromStdString(item.text)},
                                           {"level", 2}});
        }
    }
    return outline;
}

void BoardCanvas::showItem(const QString &columnId, const QString &itemId) {
    constexpr float marginPx = 24.0F;
    for (const PlacedColumn &column : pageMap_.layout().columns) {
        if (column.id != columnId.toStdString())
            continue;
        const auto item = std::ranges::find(column.items, itemId.toStdString(), &PlacedItem::id);
        const float top = item != column.items.end() ? item->bounds.top : column.bounds.top;
        const float w = static_cast<float>(width());
        const float scale =
            std::min(viewport_.scale(), (w - 2.0F * marginPx) / column.bounds.width());
        Viewport view;
        view.zoomAt({0.0F, 0.0F}, scale);
        view.panBy(w / 2.0F - (column.bounds.left + column.bounds.width() / 2.0F) * scale,
                   static_cast<float>(topInset_) + marginPx / 2.0F - top * view.scale());
        pointerCancelled();
        viewport_ = view;
        viewUpdated();
        return;
    }
}

} // namespace peerchalk
