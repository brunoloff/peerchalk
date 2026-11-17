#pragma once

#include "board/geometry.hpp"

#include <QPointF>
#include <QRectF>

namespace peerchalk {

[[nodiscard]] inline Point toPoint(QPointF point) {
    return {static_cast<float>(point.x()), static_cast<float>(point.y())};
}

[[nodiscard]] inline QPointF toQPointF(Point point) { return {point.x, point.y}; }

[[nodiscard]] inline Bounds toBounds(const QRectF &rect) {
    return {static_cast<float>(rect.left()), static_cast<float>(rect.top()),
            static_cast<float>(rect.right()), static_cast<float>(rect.bottom())};
}

[[nodiscard]] inline QRectF toRect(const Bounds &bounds) {
    return {QPointF(bounds.left, bounds.top), QPointF(bounds.right, bounds.bottom)};
}

} // namespace peerchalk
