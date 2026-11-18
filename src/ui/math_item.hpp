#pragma once

#include "math/math_engine.hpp"

#include <QColor>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

namespace peerchalk {

// Shows a LaTeX formula with smooth edges at any scale. Its implicit size is the formula's
// size. While the text does not parse, error says why and the last good formula stays
// visible. viewScale tells it about magnification by a parent, such as the board's zoom.
class MathItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString tex READ tex WRITE setTex NOTIFY texChanged)
    Q_PROPERTY(bool displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(qreal fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(qreal viewScale READ viewScale WRITE setViewScale NOTIFY viewScaleChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(int triangleCount READ triangleCount NOTIFY formulaChanged)

  public:
    explicit MathItem(QQuickItem *parent = nullptr);

    [[nodiscard]] QString tex() const { return tex_; }
    void setTex(const QString &tex);
    [[nodiscard]] bool displayMode() const { return displayMode_; }
    void setDisplayMode(bool displayMode);
    [[nodiscard]] QColor color() const { return color_; }
    void setColor(const QColor &color);
    [[nodiscard]] qreal fontSize() const { return fontSize_; }
    void setFontSize(qreal size);
    [[nodiscard]] qreal viewScale() const { return viewScale_; }
    void setViewScale(qreal scale);
    [[nodiscard]] QString error() const { return error_; }
    [[nodiscard]] int triangleCount() const;

  signals:
    void texChanged();
    void displayModeChanged();
    void colorChanged();
    void fontSizeChanged();
    void viewScaleChanged();
    void errorChanged();
    // A layout finished, successfully or not.
    void formulaChanged();

  protected:
    void componentComplete() override;
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

  private:
    void requestLayout();
    void layoutFinished(MathFormula formula);
    void updateImplicitSize();
    void refineIfNeeded();
    [[nodiscard]] qreal pixelsPerEm() const;

    QString tex_;
    bool displayMode_{true};
    QColor color_{Qt::black};
    qreal fontSize_{24.0};
    qreal viewScale_{1.0};
    QString error_;
    MathFormula formula_;
    bool meshChanged_{false};
    qreal smoothedPixelsPerEm_{0.0};
    float tolerance_; // curve accuracy of formula_, in em
    float requestedTolerance_;
    bool refining_{false}; // the current layout only adds detail
    bool refinementFailed_{false};
    bool layingOut_{false};
    bool layoutAgain_{false}; // the inputs changed during the current layout
};

} // namespace peerchalk
