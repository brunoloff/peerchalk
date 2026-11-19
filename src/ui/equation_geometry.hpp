#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>

#include <algorithm>

namespace peerchalk {

// Equations sit in text as objects that Qt centres xHeight / 2 above the baseline. Making the
// object symmetric about that line keeps the formula's own baseline on the text's.
inline constexpr qreal equationPaddingEm = 0.08;

// width, height and depth in em, as the math engine reports them.
[[nodiscard]] inline QSizeF equationObjectSize(double width, double height, double depth,
                                               qreal fontSize, qreal xHeight) {
    const qreal axis = xHeight / 2.0;
    const qreal above = height * fontSize - axis;
    const qreal below = depth * fontSize + axis;
    return {(width + 2.0 * equationPaddingEm) * fontSize, 2.0 * std::max(above, below)};
}

// The top left corner of a formula of the given height (in em) inside its object.
[[nodiscard]] inline QPointF equationOrigin(const QRectF &object, double height, qreal fontSize,
                                            qreal xHeight) {
    const qreal baseline = object.center().y() + xHeight / 2.0;
    return {object.left() + equationPaddingEm * fontSize, baseline - height * fontSize};
}

} // namespace peerchalk
