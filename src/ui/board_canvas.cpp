#include "ui/board_canvas.hpp"

#include "ui/conversions.hpp"
#include "ui/scene_nodes.hpp"

#include <QSGTransformNode>
#include <QtLogging>

#include <cmath>

namespace peerchalk {
namespace {

constexpr float gridDotSizePx = 2.4F;
constexpr float fitMarginPx = 48.0F;

} // namespace

BoardCanvas::BoardCanvas(QQuickItem *parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton);
    setAcceptTouchEvents(true);
}

void BoardCanvas::setTool(Tool tool) {
    if (tool == tool_)
        return;
    pointerCancelled();
    tool_ = tool;
    emit toolChanged();
}

void BoardCanvas::setPenColor(const QColor &color) {
    if (!color.isValid()) {
        qWarning("BoardCanvas: ignoring invalid pen colour");
        return;
    }
    if (color == penColor_)
        return;
    penColor_ = color;
    emit penColorChanged();
}

void BoardCanvas::setPenWidth(qreal width) {
    if (!std::isfinite(width) || width <= 0.0) {
        qWarning("BoardCanvas: ignoring pen width %f; it must be finite and positive", width);
        return;
    }
    if (width == penWidth_)
        return;
    penWidth_ = width;
    emit penWidthChanged();
}

void BoardCanvas::setGridColor(const QColor &color) {
    if (!color.isValid()) {
        qWarning("BoardCanvas: ignoring invalid grid colour");
        return;
    }
    if (color == gridColor_)
        return;
    gridColor_ = color;
    emit gridColorChanged();
    update();
}

int BoardCanvas::strokeCount() const { return static_cast<int>(board_.strokes().size()); }

QRectF BoardCanvas::contentBounds() const {
    const std::optional<Bounds> bounds = board_.contentBounds();
    return bounds ? toRect(*bounds) : QRectF();
}

QPointF BoardCanvas::viewCenter() const {
    const Point centre =
        viewport_.toWorld({static_cast<float>(width() / 2.0), static_cast<float>(height() / 2.0)});
    return {centre.x, centre.y};
}

QRectF BoardCanvas::visibleArea() const {
    return toRect(
        viewport_.visibleWorld(static_cast<float>(width()), static_cast<float>(height())));
}

void BoardCanvas::undo() {
    pointerCancelled();
    if (board_.undo())
        strokesUpdated();
}

void BoardCanvas::zoomBy(qreal factor) {
    if (!std::isfinite(factor) || factor <= 0.0) {
        qWarning("BoardCanvas: ignoring zoom factor %f; it must be finite and positive", factor);
        return;
    }
    viewport_.zoomAt({static_cast<float>(width() / 2.0), static_cast<float>(height() / 2.0)},
                     static_cast<float>(factor));
    viewUpdated();
}

void BoardCanvas::resetView() {
    viewport_ = Viewport{};
    viewport_.centerOn({0.0F, 0.0F}, static_cast<float>(width()), static_cast<float>(height()));
    viewUpdated();
}

void BoardCanvas::fitContent() {
    const std::optional<Bounds> content = board_.contentBounds();
    if (!content || width() <= 2.0 * fitMarginPx || height() <= 2.0 * fitMarginPx)
        return;
    viewport_.fit(*content, static_cast<float>(width()), static_cast<float>(height()), fitMarginPx,
                  1.0F);
    viewUpdated();
}

void BoardCanvas::centerOn(QPointF worldPoint) {
    if (!std::isfinite(worldPoint.x()) || !std::isfinite(worldPoint.y())) {
        qWarning("BoardCanvas: ignoring non-finite centre");
        return;
    }
    viewport_.centerOn(toPoint(worldPoint), static_cast<float>(width()),
                       static_cast<float>(height()));
    viewUpdated();
}

QSGNode *BoardCanvas::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    QSGNode *root = oldNode;
    if (root == nullptr) {
        root = new QSGNode;
        root->appendChildNode(new QSGNode); // grid
        auto *transform = new QSGTransformNode;
        transform->appendChildNode(new QSGNode); // committed strokes
        transform->appendChildNode(new QSGNode); // stroke being drawn
        root->appendChildNode(transform);
        committedDirty_ = true;
    }
    QSGNode *grid = root->firstChild();
    deleteChildren(grid);
    grid->appendChildNode(createDotsNode(
        gridDots(viewport_, static_cast<float>(width()), static_cast<float>(height())),
        gridDotSizePx, gridColor_));
    auto *transform = static_cast<QSGTransformNode *>(root->lastChild());
    transform->setMatrix(viewMatrix(viewport_));
    if (committedDirty_) {
        setStrokeNodes(transform->firstChild(), board_.strokes());
        committedDirty_ = false;
    }
    QSGNode *active = transform->lastChild();
    deleteChildren(active);
    if (activeStroke_)
        active->appendChildNode(createStrokeNode(*activeStroke_));
    return root;
}

void BoardCanvas::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    viewUpdated();
}

void BoardCanvas::viewUpdated() {
    emit viewChanged();
    update();
}

void BoardCanvas::strokesUpdated() {
    committedDirty_ = true;
    emit strokesChanged();
    update();
}

} // namespace peerchalk
