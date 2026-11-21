#pragma once

#include "board/viewport.hpp"
#include "ui/board_canvas.hpp"

#include <QColor>
#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

#include <optional>

namespace peerchalk {

// A small overview of a BoardCanvas: all strokes plus an outline of the visible area.
// Clicking or dragging centres the canvas on that spot.
class BoardMinimap : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(peerchalk::BoardCanvas *canvas READ canvas WRITE setCanvas NOTIFY canvasChanged)
    Q_PROPERTY(
        QColor viewportColor READ viewportColor WRITE setViewportColor NOTIFY viewportColorChanged)

  public:
    explicit BoardMinimap(QQuickItem *parent = nullptr);

    [[nodiscard]] BoardCanvas *canvas() const { return canvas_; }
    void setCanvas(BoardCanvas *canvas);
    [[nodiscard]] QColor viewportColor() const { return viewportColor_; }
    void setViewportColor(const QColor &color);
    // World-to-minimap mapping; empty without a canvas or room to draw.
    [[nodiscard]] std::optional<Viewport> frame() const;

  signals:
    void canvasChanged();
    void viewportColorChanged();

  protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

  private:
    void centerCanvasOn(QPointF position);
    void appendPageNodes(QSGNode *parent) const;

    QPointer<BoardCanvas> canvas_;
    QColor viewportColor_{Qt::blue};
    bool strokesDirty_{true};
};

} // namespace peerchalk
