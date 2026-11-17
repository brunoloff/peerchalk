#pragma once

#include "board/stroke.hpp"

#include <QByteArray>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

#include <vector>

namespace peerchalk {

// A static thumbnail of a saved board: every stroke, scaled to fit the item.
class BoardPreview : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QByteArray boardState READ boardState WRITE setBoardState NOTIFY boardStateChanged)

  public:
    explicit BoardPreview(QQuickItem *parent = nullptr);

    [[nodiscard]] QByteArray boardState() const { return state_; }
    // Shows nothing for an empty or unreadable state.
    void setBoardState(const QByteArray &state);

  signals:
    void boardStateChanged();

  protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

  private:
    QByteArray state_;
    std::vector<Stroke> strokes_;
    bool strokesDirty_{true};
};

} // namespace peerchalk
