#include "ui/board_preview.hpp"

#include "board/board.hpp"
#include "ui/scene_nodes.hpp"

#include <QSGTransformNode>

#include <optional>

namespace peerchalk {
namespace {

constexpr float marginPx = 12.0F;

} // namespace

BoardPreview::BoardPreview(QQuickItem *parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void BoardPreview::setBoardState(const QByteArray &state) {
    if (state == state_)
        return;
    state_ = state;
    strokes_.clear();
    if (!state.isEmpty()) {
        Board board;
        const auto *data = reinterpret_cast<const std::byte *>(state.constData());
        if (board.apply(ByteView(data, static_cast<std::size_t>(state.size()))))
            strokes_ = board.strokes();
    }
    strokesDirty_ = true;
    emit boardStateChanged();
    update();
}

void BoardPreview::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    update();
}

QSGNode *BoardPreview::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *transform = static_cast<QSGTransformNode *>(oldNode);
    if (transform == nullptr) {
        transform = new QSGTransformNode;
        strokesDirty_ = true;
    }
    std::optional<Bounds> content;
    for (const Stroke &stroke : strokes_) {
        if (const std::optional<Bounds> bounds = stroke.bounds())
            content = content ? unite(*content, *bounds) : *bounds;
    }
    if (!content || width() <= 2.0 * marginPx || height() <= 2.0 * marginPx) {
        deleteChildren(transform);
        strokesDirty_ = true;
        return transform;
    }
    Viewport fitted;
    fitted.fit(*content, static_cast<float>(width()), static_cast<float>(height()), marginPx, 1.0F);
    transform->setMatrix(viewMatrix(fitted));
    if (strokesDirty_) {
        setStrokeNodes(transform, strokes_, StrokeSelection::All);
        strokesDirty_ = false;
    }
    return transform;
}

} // namespace peerchalk
