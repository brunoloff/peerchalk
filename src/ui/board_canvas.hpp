#pragma once

#include "board/board.hpp"
#include "board/page_map.hpp"
#include "board/stroke_builder.hpp"
#include "board/viewport.hpp"
#include "ui/board_document.hpp"
#include "ui/page_overlay_model.hpp"
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
#include <set>

namespace peerchalk {

class PdfTextureNode;
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
    // How much of the top of the canvas other things cover; page boards open below it.
    Q_PROPERTY(qreal topInset MEMBER topInset_ NOTIFY topInsetChanged)
    // Page boards: what to draw over the pages in view (PageOverlayModel), and the columns
    // as maps {columnId, title, left, right, headingBottom, paper ("A4"... or ""), landscape,
    // background, pages}, left to right, for the column ruler.
    Q_PROPERTY(QAbstractItemModel *pageItems READ pageItems CONSTANT)
    Q_PROPERTY(QVariantList pageColumns READ pageColumns NOTIFY strokesChanged)
    // The page in the middle of the view: {pageId, columnId, columnTitle, number, count,
    // width, height, pdfPage (from 1; 0 for none)}, or empty. With showCurrentPage it is
    // outlined in blue.
    Q_PROPERTY(QVariantMap currentPage READ currentPage NOTIFY currentPageChanged)
    Q_PROPERTY(bool showCurrentPage READ showCurrentPage WRITE setShowCurrentPage NOTIFY
                   showCurrentPageChanged)
    // The outline of a page board: per column {columnId, itemId: "", text, level: 1, pages}
    // followed by its subheadings {columnId, itemId, text, level: 2}.
    Q_PROPERTY(QVariantList pageOutline READ pageOutline NOTIFY strokesChanged)
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
    [[nodiscard]] QAbstractItemModel *pageItems() { return &overlay_; }
    [[nodiscard]] QVariantList pageColumns() const;
    [[nodiscard]] QVariantMap currentPage() const;
    [[nodiscard]] bool showCurrentPage() const { return showCurrentPage_; }
    void setShowCurrentPage(bool show);
    [[nodiscard]] QVariantList pageOutline() const;
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
    // Page board edits (board_canvas_page_edits.cpp), by id; unknown ids are ignored and
    // nothing changes while readOnly. `before` is the item or column to insert before, or
    // "" for the end. Each is one undo step.
    Q_INVOKABLE void addPage(const QString &columnId, const QString &before);
    Q_INVOKABLE void addSubheading(const QString &columnId, const QString &before,
                                   const QString &text);
    Q_INVOKABLE void removePageItem(const QString &columnId, const QString &itemId);
    Q_INVOKABLE void setSubheadingText(const QString &columnId, const QString &itemId,
                                       const QString &text);
    // A new column takes the paper and background of the column before it (or the first).
    Q_INVOKABLE void addColumn(const QString &before);
    Q_INVOKABLE void removeColumn(const QString &columnId);
    Q_INVOKABLE void moveColumn(const QString &columnId, const QString &before);
    Q_INVOKABLE void setColumnTitle(const QString &columnId, const QString &title);
    // paper is "A4", "A5", "Letter" or "Legal"; allPages also resizes the existing pages.
    Q_INVOKABLE void setColumnPaper(const QString &columnId, const QString &paper, bool landscape,
                                    bool allPages);
    // background is "blank", "ruled", "squared" or "dotted".
    Q_INVOKABLE void setColumnBackground(const QString &columnId, const QString &background);
    // Shows a column from its heading down, at most as large as fits the width.
    Q_INVOKABLE void showColumn(const QString &columnId);
    // Moves the view to an item of a column, keeping the zoom unless the column is wider.
    Q_INVOKABLE void showItem(const QString &columnId, const QString &itemId);

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
    void topInsetChanged();
    void currentPageChanged();
    void showCurrentPageChanged();
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
    void showStartOfPages();
    [[nodiscard]] std::optional<Bounds> shownBounds() const;
    void commitStroke(const Stroke &stroke);
    std::size_t eraseAt(Point world, float radius);
    // Text boxes with their positions on the board, for boxes on pages too.
    [[nodiscard]] std::vector<TextBox> placedTextBoxes() const;
    [[nodiscard]] std::set<std::string> pagesNearView() const;
    void cullTextBoxes();
    // Where the coordinates of a box start on the board: its page's corner, or the origin.
    [[nodiscard]] Point boxOrigin(const TextBox &box) const;
    // A box at a board position, on the page there; none between pages.
    [[nodiscard]] std::optional<TextBox> newTextBoxAt(Point world) const;
    void appendPageSheets(PdfTextureNode *parent) const;
    void updateCurrentPage();
    void appendCurrentPageOutline(QSGNode *parent) const;
    // Asks the document's PDF renderer for the PDF pages in and near the view.
    void requestPdfPages();
    // PDFs may have reached this device: pages waiting for them are shown again.
    void pdfsArrived();
    [[nodiscard]] double pdfPixelsFor(const Bounds &page) const;
    [[nodiscard]] int pdfHeightFor(const Bounds &page) const;
    void appendImage(QSGNode *parent, PdfTextureNode &textures, const QImage &image,
                     const Bounds &world) const;
    void appendPdfImages(QSGNode *parent, PdfTextureNode &textures,
                         const std::vector<PlacedPage> &pages) const;
    void syncPageOverlay();
    // Runs edit on the board's pages unless readOnly, then shows the change.
    template <typename Edit> void editPages(Edit edit);

    Tool tool_{Tool::Pen};
    bool readOnly_{false};
    QColor penColor_{Qt::black};
    qreal penWidth_{3.0};
    QColor gridColor_{Qt::lightGray};
    QColor pageColor_{Qt::white};
    bool pageBoard_{false};
    bool startViewPending_{false};
    qreal topInset_{0.0};
    QString currentPageId_;
    bool showCurrentPage_{false};
    PageMap pageMap_;
    std::vector<TextBox> placedBoxes_;
    std::set<std::string> nearbyPages_; // page boards: whose text boxes QML has
    PageOverlayModel overlay_;
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
