#pragma once

#include "board/board.hpp"
#include "board/page_map.hpp"
#include "board/stroke_builder.hpp"
#include "board/viewport.hpp"
#include "ui/board_document.hpp"
#include "ui/page_scene.hpp"
#include "ui/text_box_model.hpp"

#include <QColor>
#include <QMatrix4x4>
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
// Middle- and right-button drags always pan. Text boxes are shown by QML (TextBoxLayer)
// from the textBoxes model; the Text tool creates them and picks one to edit.
// Positions exposed to QML (viewCenter, visibleArea, contentBounds) are world coordinates.
// On a page board (board_canvas_pages.cpp) the canvas shows pages in place of the grid;
// strokes and text boxes go on the page under them, in that page's coordinates.
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
    Q_PROPERTY(QColor pageColor READ pageColor WRITE setPageColor NOTIFY pageColorChanged)
    Q_PROPERTY(bool pageBoard READ pageBoard NOTIFY strokesChanged)
    Q_PROPERTY(int strokeCount READ strokeCount NOTIFY strokesChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY strokesChanged)
    // Panning and zooming only; a stroke already being drawn may finish.
    Q_PROPERTY(bool readOnly READ readOnly WRITE setReadOnly NOTIFY readOnlyChanged)
    Q_PROPERTY(QRectF contentBounds READ contentBounds NOTIFY strokesChanged)
    Q_PROPERTY(qreal zoom READ zoom NOTIFY viewChanged)
    Q_PROPERTY(QPointF viewCenter READ viewCenter NOTIFY viewChanged)
    Q_PROPERTY(QRectF visibleArea READ visibleArea NOTIFY viewChanged)
    Q_PROPERTY(QMatrix4x4 viewMatrix READ viewMatrix NOTIFY viewChanged)
    Q_PROPERTY(QAbstractItemModel *textBoxes READ textBoxes CONSTANT)
    Q_PROPERTY(QString editingTextBox READ editingTextBox NOTIFY editingTextBoxChanged)

  public:
    enum class Tool { Pen, Highlighter, Eraser, Pan, Text };
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
    [[nodiscard]] QColor pageColor() const { return pageColor_; }
    void setPageColor(const QColor &color);
    [[nodiscard]] bool pageBoard() const { return pageBoard_; }
    [[nodiscard]] const PageMap &pageMap() const { return pageMap_; }
    // What of a page board is in view and around it, for drawing.
    [[nodiscard]] PageView pageView() const;
    [[nodiscard]] int strokeCount() const;
    [[nodiscard]] bool canUndo() const { return board().canUndo(); }
    [[nodiscard]] bool readOnly() const { return readOnly_; }
    void setReadOnly(bool readOnly);
    [[nodiscard]] QRectF contentBounds() const; // null when the board is empty
    [[nodiscard]] qreal zoom() const { return viewport_.scale(); }
    [[nodiscard]] QPointF viewCenter() const;
    [[nodiscard]] QRectF visibleArea() const;
    [[nodiscard]] QMatrix4x4 viewMatrix() const;
    [[nodiscard]] QAbstractItemModel *textBoxes() { return &textBoxModel_; }
    [[nodiscard]] QString editingTextBox() const { return editingTextBox_; }
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
    // Shows all of a world area, as large as this view allows; ignores empty areas.
    Q_INVOKABLE void showArea(QRectF worldArea);
    // The item position where a world point is shown.
    Q_INVOKABLE QPointF toScreen(QPointF worldPoint) const;

    // Text boxes, by id; ids of boxes that are gone are ignored. Nothing changes while
    // readOnly. finishTextEditing removes the box if it is still empty.
    Q_INVOKABLE void editTextBox(const QString &id);
    Q_INVOKABLE void finishTextEditing();
    Q_INVOKABLE void moveTextBox(const QString &id, QPointF position);
    Q_INVOKABLE void setTextBoxWidth(const QString &id, qreal width);
    Q_INVOKABLE void setTextBoxFontSize(const QString &id, qreal size);
    Q_INVOKABLE void setTextBoxColor(const QString &id, const QColor &color);
    Q_INVOKABLE void removeTextBox(const QString &id);
    // Ends the undo step of a change made in several parts, such as a drag.
    Q_INVOKABLE void endTextBoxChange();
    void setTextBoxBody(const QString &id, const RichText &body);
    void setTextBoxHeight(const QString &id, qreal height);

  signals:
    void documentChanged();
    void readOnlyChanged();
    void toolChanged();
    void penColorChanged();
    void penWidthChanged();
    void gridColorChanged();
    void pageColorChanged();
    void strokesChanged();
    void viewChanged();
    void textBoxesChanged();
    void editingTextBoxChanged();
    // The pointer moved over the board (world coordinates), or left it.
    void cursorMoved(QPointF worldPoint, bool inside);

  protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;
    void touchEvent(QTouchEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;

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
    void updateDrafts();
    void textBoxesUpdated();
    void setEditingTextBox(const QString &id);
    void textToolPressed(QPointF position);
    [[nodiscard]] const TextBox *textBoxAt(Point world) const;
    [[nodiscard]] Board &editableBoard() { return document_->board(); }
    // Page boards (board_canvas_pages.cpp).
    void refreshPages();
    [[nodiscard]] std::optional<Bounds> shownBounds() const;
    void commitStroke(const Stroke &stroke);
    std::size_t eraseAt(Point world, float radius);
    // Text boxes with their positions on the board, for boxes on pages too.
    [[nodiscard]] std::vector<TextBox> placedTextBoxes() const;
    // Where the coordinates of a box start on the board: its page's corner, or the origin.
    [[nodiscard]] Point boxOrigin(const TextBox &box) const;
    // A box at a board position, on the page there; none between pages.
    [[nodiscard]] std::optional<TextBox> newTextBoxAt(Point world) const;
    void appendPageSheets(QSGNode *parent) const;

    Tool tool_{Tool::Pen};
    bool readOnly_{false};
    QColor penColor_{Qt::black};
    qreal penWidth_{3.0};
    QColor gridColor_{Qt::lightGray};
    QColor pageColor_{Qt::white};
    bool pageBoard_{false};
    PageMap pageMap_;
    std::vector<TextBox> placedBoxes_;
    BoardDocument ownDocument_;
    QPointer<BoardDocument> document_;
    Viewport viewport_;
    Gesture gesture_{Gesture::None};
    QPointF lastPosition_;
    std::optional<StrokeBuilder> activeStroke_;
    TextBoxModel textBoxModel_;
    QString editingTextBox_;
    StrokeLayer *highlighterLayer_;
    StrokeLayer *penLayer_;
};

} // namespace peerchalk
