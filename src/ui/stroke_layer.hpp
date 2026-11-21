#pragma once

#include "ui/page_scene.hpp"
#include "ui/scene_nodes.hpp"

#include <QQuickItem>

namespace peerchalk {

class BoardCanvas;

// Draws one selection of a BoardCanvas's strokes, including the stroke being drawn,
// through the canvas's viewport; on page boards, the strokes of the pages in view. The
// canvas creates and sizes its layers.
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
    void updateCommitted(QSGNode *committed);

    bool strokesDirty_{true};
    bool showingPages_{false};
    PageStrokeNodes pages_;
};

} // namespace peerchalk
