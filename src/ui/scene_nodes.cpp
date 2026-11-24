#include "ui/scene_nodes.hpp"

#include "board/stroke_mesh.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>

#include <algorithm>
#include <cmath>

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

namespace {

using ColoredVertices = std::vector<QSGGeometry::ColoredPoint2D>;

void appendStrokeVertices(ColoredVertices &vertices, const Stroke &stroke,
                          StrokeSelection selection, float minSpacing) {
    const QColor color = strokeColor(stroke, selection);
    const float alpha = static_cast<float>(color.alphaF());
    const auto channel = [alpha](int value) {
        return static_cast<unsigned char>(std::lround(static_cast<float>(value) * alpha));
    };
    QSGGeometry::ColoredPoint2D vertex{};
    const std::vector<MeshVertex> mesh =
        buildStrokeMesh(minSpacing > 0.0F ? simplifiedStroke(stroke, minSpacing) : stroke);
    for (const MeshVertex &point : mesh) {
        // The vertex colour material expects premultiplied colours.
        vertex.set(point.x, point.y, channel(color.red()), channel(color.green()),
                   channel(color.blue()), static_cast<unsigned char>(color.alpha()));
        vertices.push_back(vertex);
    }
}

QSGGeometryNode *createColoredTriangles(ColoredVertices::const_iterator first, std::size_t count) {
    auto *geometry =
        new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), static_cast<int>(count));
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    std::copy_n(first, count, geometry->vertexDataAsColoredPoint2D());
    auto *node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setMaterial(new QSGVertexColorMaterial);
    node->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
    return node;
}

} // namespace

QSGNode *createMergedStrokesNode(const std::vector<Stroke> &strokes, StrokeSelection selection,
                                 float minSpacing) {
    ColoredVertices vertices;
    for (const Stroke &stroke : strokes) {
        if (selects(selection, stroke) && !stroke.points().empty())
            appendStrokeVertices(vertices, stroke, selection, minSpacing);
    }
    if (vertices.empty())
        return nullptr;
    // Meshes are whole triangles, and maxMergedVertices is a multiple of 3, so every piece
    // ends on a triangle.
    auto *node = new QSGNode;
    for (std::size_t first = 0; first < vertices.size(); first += maxMergedVertices) {
        const std::size_t count = std::min<std::size_t>(maxMergedVertices, vertices.size() - first);
        node->appendChildNode(
            createColoredTriangles(vertices.cbegin() + static_cast<std::ptrdiff_t>(first), count));
    }
    return node;
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

QSGGeometryNode *createRectsNode(const std::vector<Bounds> &rects, const QColor &color) {
    QSGGeometry *geometry = createTriangles(rects.size() * 6);
    QSGGeometry::Point2D *vertex = geometry->vertexDataAsPoint2D();
    for (const Bounds &rect : rects)
        appendQuad(vertex, rect);
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

void appendTextBoxNodes(QSGNode *parent, const Board &board) {
    std::vector<Bounds> rects;
    for (const TextBox &box : board.textBoxes())
        rects.push_back(board.textBoxBounds(box));
    if (!rects.empty())
        parent->appendChildNode(createRectsNode(rects, QColor(120, 116, 108, 90)));
}

QMatrix4x4 viewMatrix(const Viewport &viewport) {
    QMatrix4x4 matrix;
    matrix.translate(viewport.offset().x, viewport.offset().y);
    matrix.scale(viewport.scale());
    return matrix;
}

} // namespace peerchalk
