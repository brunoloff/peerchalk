#include "ui/scene_nodes.hpp"

#include "board/stroke_mesh.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

namespace peerchalk {
namespace {

QSGGeometryNode *createNode(QSGGeometry *geometry, const QColor &color) {
    auto *material = new QSGFlatColorMaterial;
    material->setColor(color);
    auto *node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setMaterial(material);
    node->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
    return node;
}

QSGGeometry *createTriangles(std::size_t vertexCount) {
    auto *geometry =
        new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(vertexCount));
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    return geometry;
}

// Writes two triangles covering the rectangle and advances vertex past them.
void appendQuad(QSGGeometry::Point2D *&vertex, const Bounds &rect) {
    vertex[0].set(rect.left, rect.top);
    vertex[1].set(rect.right, rect.top);
    vertex[2].set(rect.left, rect.bottom);
    vertex[3].set(rect.right, rect.top);
    vertex[4].set(rect.right, rect.bottom);
    vertex[5].set(rect.left, rect.bottom);
    vertex += 6;
}

} // namespace

QColor strokeColor(const Stroke &stroke, StrokeSelection selection) {
    QColor color = QColor::fromRgba(stroke.argb());
    if (stroke.kind() == StrokeKind::Highlighter) {
        if (selection == StrokeSelection::HighlightersOpaque)
            color.setAlpha(255);
        else
            color.setAlphaF(color.alphaF() * highlighterOpacity);
    }
    return color;
}

bool selects(StrokeSelection selection, const Stroke &stroke) {
    if (selection == StrokeSelection::All)
        return true;
    const bool highlighter = stroke.kind() == StrokeKind::Highlighter;
    return highlighter == (selection == StrokeSelection::HighlightersOpaque);
}

QSGGeometryNode *createStrokeNode(const Stroke &stroke, const QColor &color) {
    const std::vector<MeshVertex> mesh = buildStrokeMesh(stroke);
    QSGGeometry *geometry = createTriangles(mesh.size());
    QSGGeometry::Point2D *vertices = geometry->vertexDataAsPoint2D();
    for (std::size_t i = 0; i < mesh.size(); ++i)
        vertices[i].set(mesh[i].x, mesh[i].y);
    return createNode(geometry, color);
}

QSGGeometryNode *createDotsNode(const std::vector<Point> &centres, float size,
                                const QColor &color) {
    QSGGeometry *geometry = createTriangles(centres.size() * 6);
    QSGGeometry::Point2D *vertex = geometry->vertexDataAsPoint2D();
    const float half = size / 2.0F;
    for (const Point &centre : centres)
        appendQuad(vertex, {centre.x - half, centre.y - half, centre.x + half, centre.y + half});
    return createNode(geometry, color);
}

QSGGeometryNode *createOutlineNode(const Bounds &rect, float thickness, const QColor &color) {
    QSGGeometry *geometry = createTriangles(24);
    QSGGeometry::Point2D *vertex = geometry->vertexDataAsPoint2D();
    const float t = thickness;
    appendQuad(vertex, {rect.left, rect.top, rect.right, rect.top + t});
    appendQuad(vertex, {rect.left, rect.bottom - t, rect.right, rect.bottom});
    appendQuad(vertex, {rect.left, rect.top + t, rect.left + t, rect.bottom - t});
    appendQuad(vertex, {rect.right - t, rect.top + t, rect.right, rect.bottom - t});
    return createNode(geometry, color);
}

void deleteChildren(QSGNode *parent) {
    while (QSGNode *child = parent->firstChild()) {
        parent->removeChildNode(child);
        delete child;
    }
}

void setStrokeNodes(QSGNode *parent, const std::vector<Stroke> &strokes,
                    StrokeSelection selection) {
    deleteChildren(parent);
    for (const Stroke &stroke : strokes) {
        if (selects(selection, stroke) && !stroke.points().empty())
            parent->appendChildNode(createStrokeNode(stroke, strokeColor(stroke, selection)));
    }
}

QMatrix4x4 viewMatrix(const Viewport &viewport) {
    QMatrix4x4 matrix;
    matrix.translate(viewport.offset().x, viewport.offset().y);
    matrix.scale(viewport.scale());
    return matrix;
}

} // namespace peerchalk
