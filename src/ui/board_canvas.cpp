#include "ui/board_canvas.hpp"

#include "ui/conversions.hpp"
#include "ui/scene_nodes.hpp"
#include "ui/stroke_layer.hpp"

#include <QCursor>
#include <QSGTransformNode>
#include <QtLogging>

#include <cmath>

namespace peerchalk {
namespace {

constexpr float gridDotSizePx = 2.4F;
constexpr float fitMarginPx = 48.0F;

} // namespace

BoardCanvas::BoardCanvas(QQuickItem *parent)
    : QQuickItem(parent),
      highlighterLayer_(new StrokeLayer(*this, StrokeSelection::HighlightersOpaque, this)),
      penLayer_(new StrokeLayer(*this, StrokeSelection::Pens, this)) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton);
    setAcceptTouchEvents(true);
    setAcceptHoverEvents(true);
    // Rendering the highlighters offscreen first means each pixel is covered once, so
    // the layer's opacity applies evenly however strokes overlap.
    auto *layer = highlighterLayer_->property("layer").value<QObject *>();
    if (layer == nullptr || !layer->setProperty("enabled", true) ||
        !layer->setProperty("smooth", true))
        qFatal("BoardCanvas: Qt Quick item layers are unavailable");
    highlighterLayer_->setOpacity(highlighterOpacity);
    setDocument(nullptr);
}

void BoardCanvas::setDocument(BoardDocument *document) {
    BoardDocument *next = document != nullptr ? document : &ownDocument_;
    if (next == document_)
        return;
    pointerCancelled();
    if (document_)
        finishTextEditing();
    if (document_)
        disconnect(document_, nullptr, this, nullptr);
    document_ = next;
    // Remote changes (and saved state loading) arrive through the document.
    connect(document_, &BoardDocument::changed, this, &BoardCanvas::strokesUpdated);
    connect(document_, &BoardDocument::remoteDraftsChanged, this, &BoardCanvas::updateDrafts);
    if (document_ != &ownDocument_) // that one goes away together with the canvas
        connect(document_, &QObject::destroyed, this, [this] { setDocument(nullptr); });
    strokesUpdated();
    emit documentChanged();
}

const Stroke *BoardCanvas::activeStroke() const {
    return activeStroke_ ? &activeStroke_->stroke() : nullptr;
}

void BoardCanvas::setTool(Tool tool) {
    if (tool == tool_)
        return;
    pointerCancelled();
    finishTextEditing();
    tool_ = tool;
    if (tool_ == Tool::Text)
        setCursor(Qt::IBeamCursor);
    else
        unsetCursor();
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

int BoardCanvas::strokeCount() const { return static_cast<int>(board().strokes().size()); }

QRectF BoardCanvas::contentBounds() const {
    const std::optional<Bounds> bounds = board().contentBounds();
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

void BoardCanvas::setReadOnly(bool readOnly) {
    if (readOnly == readOnly_)
        return;
    if (readOnly)
        finishTextEditing();
    readOnly_ = readOnly;
    emit readOnlyChanged();
}

void BoardCanvas::undo() {
    if (readOnly_)
        return;
    pointerCancelled();
    if (editableBoard().undo())
        redrawStrokes();
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
    const std::optional<Bounds> content = board().contentBounds();
    if (!content || width() <= 2.0 * fitMarginPx || height() <= 2.0 * fitMarginPx)
        return;
    viewport_.fit(*content, static_cast<float>(width()), static_cast<float>(height()), fitMarginPx,
                  1.0F);
    viewUpdated();
}

void BoardCanvas::showArea(QRectF worldArea) {
    if (!worldArea.isValid() || worldArea.isEmpty() || width() <= 0.0 || height() <= 0.0)
        return;
    pointerCancelled();
    viewport_.fit({static_cast<float>(worldArea.left()), static_cast<float>(worldArea.top()),
                   static_cast<float>(worldArea.right()), static_cast<float>(worldArea.bottom())},
                  static_cast<float>(width()), static_cast<float>(height()), 0.0F,
                  Viewport::maxScale);
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

QPointF BoardCanvas::toScreen(QPointF worldPoint) const {
    return toQPointF(viewport_.toScreen(toPoint(worldPoint)));
}

QSGNode *BoardCanvas::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    QSGNode *grid = oldNode != nullptr ? oldNode : new QSGNode;
    deleteChildren(grid);
    grid->appendChildNode(createDotsNode(
        gridDots(viewport_, static_cast<float>(width()), static_cast<float>(height())),
        gridDotSizePx, gridColor_));
    return grid;
}

void BoardCanvas::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    highlighterLayer_->setSize(newGeometry.size());
    penLayer_->setSize(newGeometry.size());
    viewUpdated();
}

void BoardCanvas::viewUpdated() {
    emit viewChanged();
    update();
    highlighterLayer_->update();
    penLayer_->update();
}

void BoardCanvas::strokesUpdated() {
    emit strokesChanged();
    textBoxesUpdated();
    redrawStrokes();
}

void BoardCanvas::redrawStrokes() {
    highlighterLayer_->markStrokesDirty();
    penLayer_->markStrokesDirty();
}

void BoardCanvas::updateDrafts() {
    highlighterLayer_->update();
    penLayer_->update();
}

void BoardCanvas::activeStrokeUpdated() {
    if (activeStroke_)
        document_->publishDraft(activeStroke_->stroke(), DraftState::Drawing);
    updateDrafts();
}

} // namespace peerchalk
