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

void StrokeLayer::updateCommitted(QSGNode *committed) {
    if (canvas_.pageBoard() != showingPages_) {
        deleteChildren(committed);
        pages_.reset();
        showingPages_ = canvas_.pageBoard();
        strokesDirty_ = true;
    }
    if (showingPages_) {
        // Which pages have nodes follows the view, so this runs on every frame.
        pages_.update(committed, canvas_.board(), canvas_.pageMap(), canvas_.pageView(),
                      selection_);
    } else if (strokesDirty_) {
        setStrokeNodes(committed, canvas_.board().strokes(), selection_);
    }
    strokesDirty_ = false;
}

QSGNode *StrokeLayer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *transform = static_cast<QSGTransformNode *>(oldNode);
    if (transform == nullptr) {
        transform = new QSGTransformNode;
        transform->appendChildNode(new QSGNode); // committed strokes
        transform->appendChildNode(new QSGNode); // strokes being drawn, here and elsewhere
        strokesDirty_ = true;
        showingPages_ = false;
        pages_.reset();
    }
    transform->setMatrix(viewMatrix(canvas_.viewport()));
    updateCommitted(transform->firstChild());
    QSGNode *active = transform->lastChild();
    deleteChildren(active);
    const Stroke *drawing = canvas_.activeStroke();
    if (drawing != nullptr && !drawing->points().empty() && selects(selection_, *drawing))
        active->appendChildNode(createStrokeNode(*drawing, strokeColor(*drawing, selection_)));
    for (const auto &[key, draft] : canvas_.document()->remoteDrafts())
        if (!draft.points().empty() && selects(selection_, draft))
            active->appendChildNode(createStrokeNode(draft, strokeColor(draft, selection_)));
    return transform;
}

} // namespace peerchalk
