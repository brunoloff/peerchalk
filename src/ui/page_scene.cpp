#include "ui/page_scene.hpp"

#include "board/board_pages.hpp"

#include <QSGClipNode>
#include <QSGTransformNode>

#include <cmath>

namespace peerchalk {
namespace {

// 0 at 50 % and above; n where n halvings of 50 % reach the scale.
int detailLevel(float scale) {
    return scale >= 0.5F ? 0 : static_cast<int>(std::ceil(-std::log2(scale)));
}

// Points closer than about 1.5 pixels on screen add nothing.
float pointSpacing(int detail) {
    return detail == 0 ? 0.0F : 1.5F * std::exp2(static_cast<float>(detail));
}

// The page's strokes in page coordinates, moved onto the page and clipped to it.
QSGNode *createPageNode(const Board &board, const PlacedPage &page, StrokeSelection selection,
                        int detail) {
    auto *move = new QSGTransformNode;
    QMatrix4x4 offset;
    offset.translate(page.bounds.left, page.bounds.top);
    move->setMatrix(offset);
    QSGNode *mesh =
        createMergedStrokesNode(board.pages().strokes(page.id), selection, pointSpacing(detail));
    if (mesh == nullptr)
        return move;
    auto *clip = new QSGClipNode;
    clip->setIsRectangular(true);
    clip->setClipRect(QRectF(0.0, 0.0, page.bounds.width(), page.bounds.height()));
    clip->appendChildNode(mesh);
    move->appendChildNode(clip);
    return move;
}

void drop(QSGNode *root, QSGNode *node) {
    root->removeChildNode(node);
    delete node;
}

} // namespace

void PageStrokeNodes::release(QSGNode *root, const Board &board, const PageMap &map,
                              const PageView &view, int detail) {
    for (auto it = entries_.begin(); it != entries_.end();) {
        const std::optional<PlacedPage> page = map.page(it->first);
        const bool stale = !page || page->bounds != it->second.bounds ||
                           it->second.detail != detail ||
                           board.pages().strokesVersion(it->first) != it->second.version;
        const bool far = !page || page->bounds.right < view.keep.left ||
                         page->bounds.left > view.keep.right ||
                         page->bounds.bottom < view.keep.top || page->bounds.top > view.keep.bottom;
        if (stale || far || page->bounds.height() * view.scale < minPagePixels) {
            drop(root, it->second.node);
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
}

void PageStrokeNodes::update(QSGNode *root, const Board &board, const PageMap &map,
                             const PageView &view, StrokeSelection selection) {
    const int detail = detailLevel(view.scale);
    release(root, board, map, view, detail);
    for (const PlacedPage &page : map.pagesIn(view.show)) {
        if (entries_.contains(page.id) || page.bounds.height() * view.scale < minPagePixels)
            continue;
        const std::uint64_t version = board.pages().strokesVersion(page.id);
        QSGNode *node = createPageNode(board, page, selection, detail);
        root->appendChildNode(node);
        entries_.emplace(page.id, Entry{node, version, page.bounds, detail});
    }
}

} // namespace peerchalk
