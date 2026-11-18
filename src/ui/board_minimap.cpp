#include "ui/board_minimap.hpp"

#include "ui/conversions.hpp"
#include "ui/scene_nodes.hpp"

#include <QMouseEvent>
#include <QSGTransformNode>
#include <QtLogging>

namespace peerchalk {
namespace {

constexpr float marginPx = 4.0F;
constexpr float outlinePx = 2.0F;

} // namespace

BoardMinimap::BoardMinimap(QQuickItem *parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
}

void BoardMinimap::setCanvas(BoardCanvas *canvas) {
    if (canvas == canvas_)
        return;
    if (canvas_)
        disconnect(canvas_, nullptr, this, nullptr);
    canvas_ = canvas;
    if (canvas_) {
        connect(canvas_, &BoardCanvas::strokesChanged, this, [this] {
            strokesDirty_ = true;
            update();
        });
        connect(canvas_, &BoardCanvas::viewChanged, this, &QQuickItem::update);
    }
    strokesDirty_ = true;
    emit canvasChanged();
    update();
}

void BoardMinimap::setViewportColor(const QColor &color) {
    if (!color.isValid()) {
        qWarning("BoardMinimap: ignoring invalid viewport colour");
        return;
    }
    if (color == viewportColor_)
        return;
    viewportColor_ = color;
    emit viewportColorChanged();
    update();
}

std::optional<Viewport> BoardMinimap::frame() const {
    if (!canvas_ || width() <= 2.0 * marginPx || height() <= 2.0 * marginPx)
        return std::nullopt;
    Bounds area = toBounds(canvas_->visibleArea());
    if (const std::optional<Bounds> content = canvas_->board().contentBounds())
        area = unite(area, *content);
    Viewport viewport;
    viewport.fit(area, static_cast<float>(width()), static_cast<float>(height()), marginPx,
                 Viewport::maxScale);
    return viewport;
}

QSGNode *BoardMinimap::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    QSGNode *root = oldNode;
    if (root == nullptr) {
        root = new QSGNode;
        root->appendChildNode(new QSGTransformNode); // strokes
        root->appendChildNode(new QSGNode);          // visible-area outline
        strokesDirty_ = true;
    }
    auto *strokes = static_cast<QSGTransformNode *>(root->firstChild());
    QSGNode *outline = root->lastChild();
    deleteChildren(outline);
    const std::optional<Viewport> mapping = frame();
    if (!mapping) {
        deleteChildren(strokes);
        strokesDirty_ = true;
        return root;
    }
    strokes->setMatrix(viewMatrix(*mapping));
    if (strokesDirty_) {
        setStrokeNodes(strokes, canvas_->board().strokes(), StrokeSelection::All);
        appendTextBoxNodes(strokes, canvas_->board());
        strokesDirty_ = false;
    }
    const Bounds visible = toBounds(canvas_->visibleArea());
    const Point topLeft = mapping->toScreen({visible.left, visible.top});
    const Point bottomRight = mapping->toScreen({visible.right, visible.bottom});
    outline->appendChildNode(createOutlineNode({topLeft.x, topLeft.y, bottomRight.x, bottomRight.y},
                                               outlinePx, viewportColor_));
    return root;
}

void BoardMinimap::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    update();
}

void BoardMinimap::centerCanvasOn(QPointF position) {
    if (const std::optional<Viewport> mapping = frame())
        canvas_->centerOn(toQPointF(mapping->toWorld(toPoint(position))));
}

void BoardMinimap::mousePressEvent(QMouseEvent *event) {
    centerCanvasOn(event->position());
    event->accept();
}

void BoardMinimap::mouseMoveEvent(QMouseEvent *event) {
    centerCanvasOn(event->position());
    event->accept();
}

} // namespace peerchalk
