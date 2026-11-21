#include "ui/board_preview.hpp"

#include "board/board_pages.hpp"
#include "ui/scene_nodes.hpp"

#include <QSGTransformNode>

#include <algorithm>
#include <optional>

namespace peerchalk {
namespace {

constexpr float marginPx = 12.0F;
constexpr float maxPageAreaWidth = 2600.0F; // about three A4 columns
const QColor pageEdge(0xE4, 0xE1, 0xDA);

// The top left of a page board, in the item's proportions.
std::optional<Bounds> pageArea(const PageLayout &layout, float width, float height) {
    if (layout.columns.empty())
        return std::nullopt;
    const float right = std::min(layout.columns.back().bounds.right, maxPageAreaWidth);
    return Bounds{0.0F, 0.0F, right, right * height / width};
}

// White pages with a one-pixel edge, then each page's strokes moved onto it.
void appendPages(QSGNode *parent, const Board &board, const PageLayout &layout,
                 const Bounds &visible, float scale) {
    std::vector<const PlacedItem *> pages;
    std::vector<Bounds> sheets;
    std::vector<Bounds> edges;
    const float edge = 1.0F / scale;
    for (const ItemRef ref : itemsIn(layout, visible)) {
        const PlacedItem &item = layout.columns[ref.column].items[ref.item];
        if (item.kind != ColumnItemKind::Page)
            continue;
        const Bounds &b = item.bounds;
        pages.push_back(&item);
        sheets.push_back(b);
        edges.push_back({b.left - edge, b.top - edge, b.right + edge, b.bottom + edge});
    }
    if (pages.empty())
        return;
    parent->appendChildNode(createRectsNode(edges, pageEdge));
    parent->appendChildNode(createRectsNode(sheets, Qt::white));
    for (const PlacedItem *page : pages) {
        auto *node = new QSGTransformNode;
        QMatrix4x4 offset;
        offset.translate(page->bounds.left, page->bounds.top);
        node->setMatrix(offset);
        setStrokeNodes(node, board.pages().strokes(page->id), StrokeSelection::All);
        parent->appendChildNode(node);
    }
}

} // namespace

BoardPreview::BoardPreview(QQuickItem *parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void BoardPreview::setBoardState(const QByteArray &state) {
    if (state == state_)
        return;
    state_ = state;
    board_ = std::make_unique<Board>();
    if (!state.isEmpty()) {
        const auto *data = reinterpret_cast<const std::byte *>(state.constData());
        if (!board_->apply(ByteView(data, static_cast<std::size_t>(state.size()))))
            board_ = std::make_unique<Board>();
    }
    pageBoard_ = board_->pages().kind() == BoardKind::Pages;
    const std::vector<ColumnSpec> columns =
        pageBoard_ ? board_->pages().columns() : std::vector<ColumnSpec>();
    layout_ = layoutPages(columns);
    pageCount_ = 0;
    for (const ColumnSpec &column : columns)
        pageCount_ += static_cast<int>(
            std::ranges::count(column.items, ColumnItemKind::Page, &ColumnItem::kind));
    strokesDirty_ = true;
    emit boardStateChanged();
    update();
}

void BoardPreview::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    strokesDirty_ = strokesDirty_ || pageBoard_; // which pages show depends on the size
    update();
}

QSGNode *BoardPreview::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *transform = static_cast<QSGTransformNode *>(oldNode);
    if (transform == nullptr) {
        transform = new QSGTransformNode;
        strokesDirty_ = true;
    }
    const auto w = static_cast<float>(width());
    const auto h = static_cast<float>(height());
    const std::optional<Bounds> content = !board_      ? std::nullopt
                                          : pageBoard_ ? pageArea(layout_, w, h)
                                                       : board_->contentBounds();
    if (!content || w <= 2.0F * marginPx || h <= 2.0F * marginPx) {
        deleteChildren(transform);
        strokesDirty_ = true;
        return transform;
    }
    Viewport fitted;
    fitted.fit(*content, w, h, marginPx, 1.0F);
    transform->setMatrix(viewMatrix(fitted));
    if (strokesDirty_) {
        deleteChildren(transform);
        if (pageBoard_) {
            appendPages(transform, *board_, layout_, fitted.visibleWorld(w, h), fitted.scale());
        } else {
            setStrokeNodes(transform, board_->strokes(), StrokeSelection::All);
            appendTextBoxNodes(transform, *board_);
        }
        strokesDirty_ = false;
    }
    return transform;
}

} // namespace peerchalk
