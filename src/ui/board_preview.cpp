#include "ui/board_preview.hpp"

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
    board_ = std::make_unique<Board>();
    if (!state.isEmpty()) {
        const auto *data = reinterpret_cast<const std::byte *>(state.constData());
        if (!board_->apply(ByteView(data, static_cast<std::size_t>(state.size()))))
            board_ = std::make_unique<Board>();
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
    const std::optional<Bounds> content =
        board_ ? board_->contentBounds() : std::optional<Bounds>();
    if (!content || width() <= 2.0 * marginPx || height() <= 2.0 * marginPx) {
        deleteChildren(transform);
        strokesDirty_ = true;
        return transform;
    }
    Viewport fitted;
    fitted.fit(*content, static_cast<float>(width()), static_cast<float>(height()), marginPx, 1.0F);
    transform->setMatrix(viewMatrix(fitted));
    if (strokesDirty_) {
        setStrokeNodes(transform, board_->strokes(), StrokeSelection::All);
        appendTextBoxNodes(transform, *board_);
        strokesDirty_ = false;
    }
    return transform;
}

} // namespace peerchalk
