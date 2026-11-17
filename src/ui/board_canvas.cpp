#include "ui/board_canvas.hpp"

#include "board/stroke_mesh.hpp"

#include <QEventPoint>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QTouchEvent>
#include <QtLogging>

#include <algorithm>
#include <cmath>

namespace peerchalk {
namespace {

float pressureOf(const QEventPoint &point) {
    const QPointingDevice *device = point.device();
    if (device == nullptr || !device->capabilities().testFlag(QInputDevice::Capability::Pressure))
        return 1.0F;
    return std::clamp(static_cast<float>(point.pressure()), 0.0F, 1.0F);
}

QSGGeometryNode *createStrokeNode(const Stroke &stroke) {
    const std::vector<MeshVertex> strip = buildTriangleStrip(stroke);
    auto *geometry =
        new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(strip.size()));
    geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    QSGGeometry::Point2D *vertices = geometry->vertexDataAsPoint2D();
    for (std::size_t i = 0; i < strip.size(); ++i)
        vertices[i].set(strip[i].x, strip[i].y);
    auto *material = new QSGFlatColorMaterial;
    material->setColor(QColor::fromRgba(stroke.argb()));
    auto *node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setMaterial(material);
    node->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
    return node;
}

} // namespace

BoardCanvas::BoardCanvas(QQuickItem *parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptTouchEvents(true);
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

int BoardCanvas::strokeCount() const { return static_cast<int>(strokes_.size()); }

void BoardCanvas::clear() {
    activeStroke_.reset();
    if (strokes_.empty())
        return;
    strokes_.clear();
    emit strokesChanged();
    update();
}

QSGNode *BoardCanvas::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    QSGNode *root = oldNode != nullptr ? oldNode : new QSGNode;
    root->removeAllChildNodes();
    for (const Stroke &stroke : strokes_)
        root->appendChildNode(createStrokeNode(stroke));
    if (activeStroke_)
        root->appendChildNode(createStrokeNode(*activeStroke_));
    return root;
}

void BoardCanvas::beginStroke(const QEventPoint &point) {
    activeStroke_.emplace(penColor_.rgba(), static_cast<float>(penWidth_));
    extendStroke(point);
}

void BoardCanvas::extendStroke(const QEventPoint &point) {
    if (!activeStroke_)
        return;
    const QPointF position = point.position();
    activeStroke_->addPoint(
        {static_cast<float>(position.x()), static_cast<float>(position.y()), pressureOf(point)});
    update();
}

void BoardCanvas::finishStroke() {
    if (!activeStroke_)
        return;
    strokes_.push_back(std::move(*activeStroke_));
    activeStroke_.reset();
    emit strokesChanged();
    update();
}

void BoardCanvas::mousePressEvent(QMouseEvent *event) {
    beginStroke(event->point(0));
    event->accept();
}

void BoardCanvas::mouseMoveEvent(QMouseEvent *event) {
    extendStroke(event->point(0));
    event->accept();
}

void BoardCanvas::mouseReleaseEvent(QMouseEvent *event) {
    extendStroke(event->point(0));
    finishStroke();
    event->accept();
}

void BoardCanvas::touchEvent(QTouchEvent *event) {
    if (event->type() == QEvent::TouchCancel || event->points().isEmpty()) {
        activeStroke_.reset();
        update();
        return;
    }
    const QEventPoint &point = event->points().front();
    if (point.state() == QEventPoint::Pressed)
        beginStroke(point);
    else if (point.state() == QEventPoint::Updated)
        extendStroke(point);
    else if (point.state() == QEventPoint::Released) {
        extendStroke(point);
        finishStroke();
    }
    event->accept();
}

} // namespace peerchalk
