#pragma once

#include "board/stroke.hpp"

#include <QColor>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

#include <optional>
#include <vector>

class QEventPoint;

namespace peerchalk {

// Draws the board's strokes with the Qt Quick scene graph and records new strokes
// from mouse, touch and pen input.
class BoardCanvas : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QColor penColor READ penColor WRITE setPenColor NOTIFY penColorChanged)
    Q_PROPERTY(qreal penWidth READ penWidth WRITE setPenWidth NOTIFY penWidthChanged)
    Q_PROPERTY(int strokeCount READ strokeCount NOTIFY strokesChanged)

  public:
    explicit BoardCanvas(QQuickItem *parent = nullptr);

    [[nodiscard]] QColor penColor() const { return penColor_; }
    void setPenColor(const QColor &color);
    [[nodiscard]] qreal penWidth() const { return penWidth_; }
    void setPenWidth(qreal width);
    [[nodiscard]] int strokeCount() const;
    [[nodiscard]] const std::vector<Stroke> &strokes() const { return strokes_; }

    Q_INVOKABLE void clear();

  signals:
    void penColorChanged();
    void penWidthChanged();
    void strokesChanged();

  protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void touchEvent(QTouchEvent *event) override;

  private:
    void beginStroke(const QEventPoint &point);
    void extendStroke(const QEventPoint &point);
    void finishStroke();

    QColor penColor_{Qt::black};
    qreal penWidth_{3.0};
    std::vector<Stroke> strokes_;
    std::optional<Stroke> activeStroke_;
};

} // namespace peerchalk
