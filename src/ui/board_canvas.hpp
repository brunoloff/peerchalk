#pragma once

#include "board/board.hpp"
#include "board/viewport.hpp"

#include <QColor>
#include <QPointF>
#include <QQuickItem>
#include <QRectF>
#include <QtQml/qqmlregistration.h>

#include <optional>

namespace peerchalk {

// The infinite board: draws strokes and a dotted grid through a pannable, zoomable
// viewport, and turns mouse, touch and pen input into drawing, erasing and panning.
// Positions exposed to QML (viewCenter, visibleArea, contentBounds) are world coordinates.
class BoardCanvas : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
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
    [[nodiscard]] bool canUndo() const { return board_.canUndo(); }
    [[nodiscard]] QRectF contentBounds() const; // null when the board is empty
    [[nodiscard]] qreal zoom() const { return viewport_.scale(); }
    [[nodiscard]] QPointF viewCenter() const;
    [[nodiscard]] QRectF visibleArea() const;
    [[nodiscard]] const Board &board() const { return board_; }

    Q_INVOKABLE void undo();
    // Zooms about the centre of the view by factor (> 0).
    Q_INVOKABLE void zoomBy(qreal factor);
    // Shows the world origin at 100%.
    Q_INVOKABLE void resetView();
    // Shows every stroke, zooming in no further than 100%; does nothing on an empty board.
    Q_INVOKABLE void fitContent();
    Q_INVOKABLE void centerOn(QPointF worldPoint);

  signals:
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

    void pointerPressed(QPointF position, float pressure, bool forcePan);
    void pointerMoved(QPointF position, float pressure);
    void pointerReleased(QPointF position, float pressure);
    void pointerCancelled();
    void addActivePoint(QPointF position, float pressure);
    void eraseAlong(QPointF from, QPointF to);
    void viewUpdated();
    void strokesUpdated();

    Tool tool_{Tool::Pen};
    QColor penColor_{Qt::black};
    qreal penWidth_{3.0};
    QColor gridColor_{Qt::lightGray};
    Board board_;
    Viewport viewport_;
    Gesture gesture_{Gesture::None};
    QPointF lastPosition_;
    std::optional<Stroke> activeStroke_;
    bool committedDirty_{true};
};

} // namespace peerchalk
