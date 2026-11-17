#pragma once

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

[[nodiscard]] QSGGeometryNode *createStrokeNode(const Stroke &stroke);
// Square dots of the given size centred on each point.
[[nodiscard]] QSGGeometryNode *createDotsNode(const std::vector<Point> &centres, float size,
                                              const QColor &color);
// A rectangle outline drawn inside rect with the given line thickness.
[[nodiscard]] QSGGeometryNode *createOutlineNode(const Bounds &rect, float thickness,
                                                 const QColor &color);

// Removes and deletes every child of parent.
void deleteChildren(QSGNode *parent);
// Replaces parent's children with one node per stroke.
void setStrokeNodes(QSGNode *parent, const std::vector<Stroke> &strokes);
[[nodiscard]] QMatrix4x4 viewMatrix(const Viewport &viewport);

} // namespace peerchalk
