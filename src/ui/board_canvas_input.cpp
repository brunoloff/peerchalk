// Pointer, touch and wheel handling for BoardCanvas.
#include "ui/board_canvas.hpp"

#include "ui/conversions.hpp"

#include <QEventPoint>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QTouchEvent>
#include <QWheelEvent>

#include <cmath>

namespace peerchalk {
namespace {

constexpr float eraserRadiusPx = 10.0F;
constexpr float highlighterAlpha = 0.35F;
constexpr float highlighterWidthFactor = 4.0F;
constexpr float wheelZoomBase = 1.0015F; // per eighth of a degree of wheel rotation
constexpr qreal wheelPanPerEighthDegree = 0.5;

float pressureOf(const QEventPoint &point) {
    const QPointingDevice *device = point.device();
    if (device == nullptr || !device->capabilities().testFlag(QInputDevice::Capability::Pressure))
        return 1.0F;
    return std::clamp(static_cast<float>(point.pressure()), 0.0F, 1.0F);
}

} // namespace

void BoardCanvas::pointerPressed(QPointF position, float pressure, bool forcePan) {
    if (gesture_ != Gesture::None)
        return;
    const Tool tool = forcePan ? Tool::Pan : tool_;
    lastPosition_ = position;
    if (tool == Tool::Pan) {
        gesture_ = Gesture::Panning;
    } else if (tool == Tool::Eraser) {
        gesture_ = Gesture::Erasing;
        eraseAlong(position, position);
    } else {
        gesture_ = Gesture::Drawing;
        QColor color = penColor_;
        float width = static_cast<float>(penWidth_);
        if (tool == Tool::Highlighter) {
            color.setAlphaF(highlighterAlpha);
            width *= highlighterWidthFactor;
        }
        activeStroke_.emplace(color.rgba(), width);
        addActivePoint(position, pressure);
    }
}

void BoardCanvas::pointerMoved(QPointF position, float pressure) {
    if (gesture_ == Gesture::Panning) {
        const QPointF delta = position - lastPosition_;
        lastPosition_ = position;
        viewport_.panBy(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
        viewUpdated();
    } else if (gesture_ == Gesture::Erasing) {
        eraseAlong(lastPosition_, position);
        lastPosition_ = position;
    } else if (gesture_ == Gesture::Drawing) {
        addActivePoint(position, pressure);
    }
}

void BoardCanvas::pointerReleased(QPointF position, float pressure) {
    pointerMoved(position, pressure);
    if (gesture_ == Gesture::Drawing && activeStroke_) {
        board_.addStroke(std::move(*activeStroke_));
        activeStroke_.reset();
        strokesUpdated();
    }
    board_.endGesture();
    gesture_ = Gesture::None;
}

void BoardCanvas::pointerCancelled() {
    if (gesture_ == Gesture::None)
        return;
    activeStroke_.reset();
    board_.endGesture();
    gesture_ = Gesture::None;
    update();
}

void BoardCanvas::addActivePoint(QPointF position, float pressure) {
    const Point world = viewport_.toWorld(toPoint(position));
    activeStroke_->addPoint({world.x, world.y, pressure});
    update();
}

// Erases at points no more than half the eraser radius apart from `from` to `to`, so fast
// drags do not skip strokes between pointer events.
void BoardCanvas::eraseAlong(QPointF from, QPointF to) {
    const QPointF delta = to - from;
    const qreal length = std::hypot(delta.x(), delta.y());
    const int steps = std::max(1, static_cast<int>(std::ceil(length / (eraserRadiusPx / 2.0))));
    const float radius = eraserRadiusPx / viewport_.scale();
    std::size_t removed = 0;
    for (int step = 0; step <= steps; ++step) {
        const QPointF sample = from + delta * (static_cast<qreal>(step) / steps);
        removed += board_.eraseNear(viewport_.toWorld(toPoint(sample)), radius);
    }
    if (removed > 0)
        strokesUpdated();
}

void BoardCanvas::mousePressEvent(QMouseEvent *event) {
    pointerPressed(event->position(), pressureOf(event->point(0)),
                   event->button() == Qt::MiddleButton);
    event->accept();
}

void BoardCanvas::mouseMoveEvent(QMouseEvent *event) {
    pointerMoved(event->position(), pressureOf(event->point(0)));
    event->accept();
}

void BoardCanvas::mouseReleaseEvent(QMouseEvent *event) {
    pointerReleased(event->position(), pressureOf(event->point(0)));
    event->accept();
}

void BoardCanvas::mouseUngrabEvent() { pointerCancelled(); }

void BoardCanvas::touchEvent(QTouchEvent *event) {
    if (event->type() == QEvent::TouchCancel || event->points().isEmpty()) {
        pointerCancelled();
        return;
    }
    const QEventPoint &point = event->points().front();
    if (point.state() == QEventPoint::Pressed)
        pointerPressed(point.position(), pressureOf(point), false);
    else if (point.state() == QEventPoint::Updated)
        pointerMoved(point.position(), pressureOf(point));
    else if (point.state() == QEventPoint::Released)
        pointerReleased(point.position(), pressureOf(point));
    event->accept();
}

void BoardCanvas::wheelEvent(QWheelEvent *event) {
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const float factor = std::pow(wheelZoomBase, static_cast<float>(event->angleDelta().y()));
        viewport_.zoomAt(toPoint(event->position()), factor);
    } else {
        const QPointF delta = event->pixelDelta().isNull()
                                  ? QPointF(event->angleDelta()) * wheelPanPerEighthDegree
                                  : QPointF(event->pixelDelta());
        viewport_.panBy(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
    }
    viewUpdated();
    event->accept();
}

} // namespace peerchalk
