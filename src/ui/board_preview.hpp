#pragma once

#include "board/board.hpp"
#include "board/page_layout.hpp"

#include <QByteArray>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace peerchalk {

// A static thumbnail of a saved board: every stroke and text box position, scaled to fit the
// item. A page board shows its top left, its pages and their strokes, as wide as a few
// columns at most, and tells how many columns and pages it has.
class BoardPreview : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QByteArray boardState READ boardState WRITE setBoardState NOTIFY boardStateChanged)
    Q_PROPERTY(bool pageBoard READ pageBoard NOTIFY boardStateChanged)
    Q_PROPERTY(int columnCount READ columnCount NOTIFY boardStateChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY boardStateChanged)

  public:
    explicit BoardPreview(QQuickItem *parent = nullptr);

    [[nodiscard]] QByteArray boardState() const { return state_; }
    // Shows nothing for an empty or unreadable state.
    void setBoardState(const QByteArray &state);
    [[nodiscard]] bool pageBoard() const { return pageBoard_; }
    [[nodiscard]] int columnCount() const { return static_cast<int>(layout_.columns.size()); }
    [[nodiscard]] int pageCount() const { return pageCount_; }

  signals:
    void boardStateChanged();

  protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

  private:
    QByteArray state_;
    std::unique_ptr<Board> board_;
    bool strokesDirty_{true};
    bool pageBoard_{false};
    PageLayout layout_;
    int pageCount_{0};
};

} // namespace peerchalk
