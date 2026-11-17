#pragma once

#include "ui/scene_nodes.hpp"

#include <QQuickItem>

namespace peerchalk {

class BoardCanvas;

// Draws one selection of a BoardCanvas's strokes, including the stroke being drawn,
// through the canvas's viewport. The canvas creates and sizes its layers.
class StrokeLayer : public QQuickItem {
    Q_OBJECT

  public:
    StrokeLayer(const BoardCanvas &canvas, StrokeSelection selection, QQuickItem *parent);

    // Rebuilds the committed strokes on the next frame.
    void markStrokesDirty();

  protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

  private:
    const BoardCanvas &canvas_;
    StrokeSelection selection_;
    bool strokesDirty_{true};
};

} // namespace peerchalk
