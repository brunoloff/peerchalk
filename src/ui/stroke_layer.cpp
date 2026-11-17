#include "ui/stroke_layer.hpp"

#include "ui/board_canvas.hpp"

#include <QSGTransformNode>

namespace peerchalk {

StrokeLayer::StrokeLayer(const BoardCanvas &canvas, StrokeSelection selection, QQuickItem *parent)
    : QQuickItem(parent), canvas_(canvas), selection_(selection) {
    setFlag(ItemHasContents, true);
}

void StrokeLayer::markStrokesDirty() {
    strokesDirty_ = true;
    update();
}

QSGNode *StrokeLayer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *transform = static_cast<QSGTransformNode *>(oldNode);
    if (transform == nullptr) {
        transform = new QSGTransformNode;
        transform->appendChildNode(new QSGNode); // committed strokes
        transform->appendChildNode(new QSGNode); // stroke being drawn
        strokesDirty_ = true;
    }
    transform->setMatrix(viewMatrix(canvas_.viewport()));
    if (strokesDirty_) {
        setStrokeNodes(transform->firstChild(), canvas_.board().strokes(), selection_);
        strokesDirty_ = false;
    }
    QSGNode *active = transform->lastChild();
    deleteChildren(active);
    const Stroke *drawing = canvas_.activeStroke();
    if (drawing != nullptr && !drawing->points().empty() && selects(selection_, *drawing))
        active->appendChildNode(createStrokeNode(*drawing, strokeColor(*drawing, selection_)));
    return transform;
}

} // namespace peerchalk
