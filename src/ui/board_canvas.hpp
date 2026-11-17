#pragma once

#include "board/board.hpp"
#include "board/stroke_builder.hpp"
#include "board/viewport.hpp"
#include "ui/board_document.hpp"

#include <QColor>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QRectF>
#include <QtQml/qqmlregistration.h>

#include <optional>

namespace peerchalk {

class StrokeLayer;

// The infinite board: draws a dotted grid and, through two child layers, the strokes,
// all through a pannable, zoomable viewport. Highlighter strokes are drawn opaque into
// an offscreen layer shown at highlighterOpacity beneath the pen ink, so overlapping
// highlighter strokes never darken and ink stays crisp. Turns mouse, touch and pen input
// (with pressure when the device reports it) into drawing, erasing and panning.
// Middle- and right-button drags always pan.
// Positions exposed to QML (viewCenter, visibleArea, contentBounds) are world coordinates.
class BoardCanvas : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    // The board shown and edited; without one the canvas uses a private empty board.
    Q_PROPERTY(
        peerchalk::BoardDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(Tool tool READ tool WRITE setTool NOTIFY toolChanged)
    Q_PROPERTY(QColor penColor READ penColor WRITE setPenColor NOTIFY penColorChanged)
    Q_PROPERTY(qreal penWidth READ penWidth WRITE setPenWidth NOTIFY penWidthChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY gridColorChanged)
    Q_PROPERTY(int strokeCount READ strokeCount NOTIFY strokesChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY strokesChanged)
    Q_PROPERTY(QRectF contentBounds READ contentBounds NOTIFY strokesChanged)
    Q_PROPERTY(qreal zoom READ zoom NOTIFY viewChanged)
    Q_PROPERTY(QPointF viewCenter READ viewCenter NOTIFY viewChanged)
    Q_PROPERTY(QRectF visibleArea READ visibleArea NOTIFY viewChanged)

  public:
    enum class Tool { Pen, Highlighter, Eraser, Pan };
    Q_ENUM(Tool)

    explicit BoardCanvas(QQuickItem *parent = nullptr);

    [[nodiscard]] Tool tool() const { return tool_; }
    void setTool(Tool tool);
    [[nodiscard]] QColor penColor() const { return penColor_; }
    void setPenColor(const QColor &color);
    [[nodiscard]] qreal penWidth() const { return penWidth_; }
    void setPenWidth(qreal width);
    [[nodiscard]] QColor gridColor() const { return gridColor_; }
    void setGridColor(const QColor &color);
    [[nodiscard]] int strokeCount() const;
    [[nodiscard]] bool canUndo() const { return board().canUndo(); }
    [[nodiscard]] QRectF contentBounds() const; // null when the board is empty
    [[nodiscard]] qreal zoom() const { return viewport_.scale(); }
    [[nodiscard]] QPointF viewCenter() const;
    [[nodiscard]] QRectF visibleArea() const;
    [[nodiscard]] BoardDocument *document() const { return document_; }
    void setDocument(BoardDocument *document);
    [[nodiscard]] const Board &board() const { return document_->board(); }
    [[nodiscard]] const Viewport &viewport() const { return viewport_; }
    // The stroke being drawn, or null.
    [[nodiscard]] const Stroke *activeStroke() const;

    Q_INVOKABLE void undo();
    // Zooms about the centre of the view by factor (> 0).
    Q_INVOKABLE void zoomBy(qreal factor);
    // Shows the world origin at 100%.
    Q_INVOKABLE void resetView();
    // Shows every stroke, zooming in no further than 100%; does nothing on an empty board.
    Q_INVOKABLE void fitContent();
    Q_INVOKABLE void centerOn(QPointF worldPoint);

  signals:
    void documentChanged();
    void toolChanged();
    void penColorChanged();
    void penWidthChanged();
    void gridColorChanged();
    void strokesChanged();
    void viewChanged();

  protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;
    void touchEvent(QTouchEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

  private:
    enum class Gesture { None, Drawing, Erasing, Panning };

    // pressure is empty for devices without a pressure sensor.
    void pointerPressed(QPointF position, std::optional<float> pressure, bool forcePan);
    void pointerMoved(QPointF position, std::optional<float> pressure);
    void pointerReleased(QPointF position, std::optional<float> pressure);
    void pointerCancelled();
    void addActivePoint(QPointF position, std::optional<float> pressure);
    void eraseAlong(QPointF from, QPointF to);
    void viewUpdated();
    void strokesUpdated();
    // Redraws committed strokes now, after a local edit; the document's `changed`
    // notification follows and announces the change (strokesChanged).
    void redrawStrokes();
    void activeStrokeUpdated();
    [[nodiscard]] Board &editableBoard() { return document_->board(); }

    Tool tool_{Tool::Pen};
    QColor penColor_{Qt::black};
    qreal penWidth_{3.0};
    QColor gridColor_{Qt::lightGray};
    BoardDocument ownDocument_;
    QPointer<BoardDocument> document_;
    Viewport viewport_;
    Gesture gesture_{Gesture::None};
    QPointF lastPosition_;
    std::optional<StrokeBuilder> activeStroke_;
    StrokeLayer *highlighterLayer_;
    StrokeLayer *penLayer_;
};

} // namespace peerchalk
