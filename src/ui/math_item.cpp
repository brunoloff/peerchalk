#include "ui/math_item.hpp"

#include "math/math_smoothing.hpp"
#include "ui/scene_nodes.hpp"

#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGTransformNode>
#include <QSGVertexColorMaterial>
#include <QtLogging>

#include <algorithm>
#include <cmath>
#include <utility>

namespace peerchalk {
namespace {

constexpr float coarsestTolerance = 0.004F;
constexpr float finestTolerance = 0.000125F;
constexpr qreal curveErrorPixels = 0.2;
constexpr qreal resmoothRatio = 1.2;

uchar channel(quint32 value, float alpha) { return uchar(std::lround(float(value) * alpha)); }

// Vertex colours are premultiplied, which lets one run fade out towards its edges.
QSGGeometryNode *createRunNode(const MathFormula &formula, const MathRun &run, float pixelEm) {
    const SmoothMesh mesh = smoothMathRun(formula, run, pixelEm);
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(),
                                     int(mesh.vertices.size()), int(mesh.indices.size()),
                                     QSGGeometry::UnsignedIntType);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    const quint32 red = run.rgba >> 24;
    const quint32 green = run.rgba >> 16 & 0xff;
    const quint32 blue = run.rgba >> 8 & 0xff;
    const float opacity = float(run.rgba & 0xff) / 255.0F;
    QSGGeometry::ColoredPoint2D *out = geometry->vertexDataAsColoredPoint2D();
    for (const SmoothVertex &vertex : mesh.vertices) {
        const float alpha = vertex.alpha * opacity;
        (out++)->set(vertex.x, vertex.y, channel(red, alpha), channel(green, alpha),
                     channel(blue, alpha), uchar(std::lround(alpha * 255.0F)));
    }
    std::copy(mesh.indices.begin(), mesh.indices.end(), geometry->indexDataAsUInt());
    auto *node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setMaterial(new QSGVertexColorMaterial);
    node->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
    return node;
}

// The coarsest of a few accuracies that keeps curves within a fraction of a pixel.
float toleranceFor(qreal pixelsPerEm) {
    float tolerance = coarsestTolerance;
    while (tolerance > finestTolerance && tolerance * pixelsPerEm > curveErrorPixels)
        tolerance /= 2.0F;
    return tolerance;
}

} // namespace

MathItem::MathItem(QQuickItem *parent)
    : QQuickItem(parent), tolerance_(coarsestTolerance), requestedTolerance_(coarsestTolerance) {
    setFlag(ItemHasContents, true);
    connect(this, &QQuickItem::scaleChanged, this, &MathItem::refineIfNeeded);
}

void MathItem::setTex(const QString &tex) {
    if (tex == tex_)
        return;
    tex_ = tex;
    emit texChanged();
    refining_ = false;
    refinementFailed_ = false;
    requestLayout();
}

void MathItem::setDisplayMode(bool displayMode) {
    if (displayMode == displayMode_)
        return;
    displayMode_ = displayMode;
    emit displayModeChanged();
    requestLayout();
}

void MathItem::setColor(const QColor &color) {
    if (!color.isValid()) {
        qWarning("MathItem: ignoring invalid colour");
        return;
    }
    if (color == color_)
        return;
    color_ = color;
    emit colorChanged();
    requestLayout();
}

void MathItem::setFontSize(qreal size) {
    if (!std::isfinite(size) || size <= 0.0) {
        qWarning("MathItem: ignoring font size %f; it must be finite and positive", size);
        return;
    }
    if (size == fontSize_)
        return;
    fontSize_ = size;
    emit fontSizeChanged();
    updateImplicitSize();
    refineIfNeeded();
}

void MathItem::setViewScale(qreal scale) {
    if (!std::isfinite(scale) || scale <= 0.0) {
        qWarning("MathItem: ignoring view scale %f; it must be finite and positive", scale);
        return;
    }
    if (scale == viewScale_)
        return;
    viewScale_ = scale;
    emit viewScaleChanged();
    refineIfNeeded();
}

qreal MathItem::pixelsPerEm() const {
    const qreal ratio = window() != nullptr ? window()->effectiveDevicePixelRatio() : 1.0;
    return fontSize_ * viewScale_ * scale() * ratio;
}

// Zooming in asks for finer curves; what is already fine enough is kept when zooming out.
void MathItem::refineIfNeeded() {
    update();
    const float wanted = toleranceFor(pixelsPerEm());
    if (refinementFailed_ || layingOut_ || wanted >= tolerance_ || tex_.isEmpty())
        return;
    requestedTolerance_ = wanted;
    refining_ = true;
    requestLayout();
}

void MathItem::itemChange(ItemChange change, const ItemChangeData &value) {
    QQuickItem::itemChange(change, value);
    if (change == ItemSceneChange || change == ItemDevicePixelRatioHasChanged)
        refineIfNeeded();
}

int MathItem::triangleCount() const { return int(formula_.indices.size() / 3); }

void MathItem::componentComplete() {
    QQuickItem::componentComplete();
    requestLayout();
}

void MathItem::requestLayout() {
    if (!isComponentComplete())
        return;
    if (layingOut_) {
        layoutAgain_ = true;
        return;
    }
    layingOut_ = true;
    if (!refining_)
        requestedTolerance_ = toleranceFor(pixelsPerEm());
    layoutMath({tex_, displayMode_, color_.rgba(), requestedTolerance_}, this,
               [this](MathFormula formula) { layoutFinished(std::move(formula)); });
}

void MathItem::layoutFinished(MathFormula formula) {
    layingOut_ = false;
    const bool refinement = std::exchange(refining_, false);
    if (std::exchange(layoutAgain_, false)) {
        requestLayout();
        return;
    }
    if (refinement && !formula.error.isEmpty()) {
        refinementFailed_ = true; // too detailed to draw: keep what is shown
        return;
    }
    if (formula.error != error_) {
        error_ = formula.error;
        emit errorChanged();
    }
    if (formula.error.isEmpty()) {
        formula_ = std::move(formula);
        tolerance_ = requestedTolerance_;
        meshChanged_ = true;
        updateImplicitSize();
        update();
    }
    emit formulaChanged();
    refineIfNeeded();
}

void MathItem::updateImplicitSize() {
    setImplicitSize(formula_.width * fontSize_, (formula_.height + formula_.depth) * fontSize_);
}

QSGNode *MathItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *root = static_cast<QSGTransformNode *>(oldNode);
    if (root == nullptr)
        root = new QSGTransformNode;
    const qreal perEm = pixelsPerEm();
    const qreal ratio = smoothedPixelsPerEm_ > 0.0 ? perEm / smoothedPixelsPerEm_ : 0.0;
    const bool bandOff = ratio < 1.0 / resmoothRatio || ratio > resmoothRatio;
    if (std::exchange(meshChanged_, false) || bandOff) {
        deleteChildren(root);
        smoothedPixelsPerEm_ = perEm;
        for (const MathRun &run : formula_.runs)
            root->appendChildNode(createRunNode(formula_, run, float(1.0 / perEm)));
    }
    QMatrix4x4 matrix;
    matrix.scale(float(fontSize_));
    root->setMatrix(matrix);
    return root;
}

} // namespace peerchalk
