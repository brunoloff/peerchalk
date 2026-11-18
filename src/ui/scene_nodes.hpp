#pragma once

#include "board/board.hpp"
#include "board/stroke.hpp"
#include "board/viewport.hpp"

#include <QColor>
#include <QMatrix4x4>

#include <vector>

class QSGNode;
class QSGGeometryNode;

namespace peerchalk {

// Scene-graph builders shared by the board canvas and its minimap. Every node returned
// owns its geometry and material; parents own their children.

// Highlighter strokes are shown at this opacity over the board.
inline constexpr float highlighterOpacity = 0.47F;

// Which strokes a set of nodes shows, and how highlighters are coloured.
enum class StrokeSelection {
    All,               // every stroke; highlighters translucent (for overviews)
    Pens,              // pen strokes only
    HighlightersOpaque // highlighters only, opaque, for a layer shown at highlighterOpacity
};

[[nodiscard]] QColor strokeColor(const Stroke &stroke, StrokeSelection selection);
[[nodiscard]] bool selects(StrokeSelection selection, const Stroke &stroke);
[[nodiscard]] QSGGeometryNode *createStrokeNode(const Stroke &stroke, const QColor &color);
// Square dots of the given size centred on each point.
[[nodiscard]] QSGGeometryNode *createDotsNode(const std::vector<Point> &centres, float size,
                                              const QColor &color);
// A rectangle outline drawn inside rect with the given line thickness.
[[nodiscard]] QSGGeometryNode *createOutlineNode(const Bounds &rect, float thickness,
                                                 const QColor &color);

// Filled rectangles in one colour.
[[nodiscard]] QSGGeometryNode *createRectsNode(const std::vector<Bounds> &rects,
                                               const QColor &color);

// Removes and deletes every child of parent.
void deleteChildren(QSGNode *parent);
// Replaces parent's children with one node per selected stroke that has points.
void setStrokeNodes(QSGNode *parent, const std::vector<Stroke> &strokes, StrokeSelection selection);
// Adds a block standing for each text box, for overviews of the board.
void appendTextBoxNodes(QSGNode *parent, const Board &board);
[[nodiscard]] QMatrix4x4 viewMatrix(const Viewport &viewport);

} // namespace peerchalk
