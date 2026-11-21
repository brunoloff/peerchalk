#include "ui/page_scene.hpp"

#include "board/board_pages.hpp"

#include <QSGClipNode>
#include <QSGGeometryNode>
#include <QSGTransformNode>

#include <iterator>

namespace peerchalk {
namespace {

bool sameBounds(const Bounds &a, const Bounds &b) {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

// The page's strokes in page coordinates, moved onto the page and clipped to it.
QSGNode *createPageNode(const Board &board, const PlacedPage &page, StrokeSelection selection) {
    auto *move = new QSGTransformNode;
    QMatrix4x4 offset;
    offset.translate(page.bounds.left, page.bounds.top);
    move->setMatrix(offset);
    QSGGeometryNode *mesh = createMergedStrokesNode(board.pages().strokes(page.id), selection);
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
                              const PageView &view) {
    for (auto it = entries_.begin(); it != entries_.end();) {
        const std::optional<PlacedPage> page = map.page(it->first);
        const bool stale = !page || !sameBounds(page->bounds, it->second.bounds) ||
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
    release(root, board, map, view);
    for (const PlacedPage &page : map.pagesIn(view.show)) {
        if (entries_.contains(page.id) || page.bounds.height() * view.scale < minPagePixels)
            continue;
        const std::uint64_t version = board.pages().strokesVersion(page.id);
        QSGNode *node = createPageNode(board, page, selection);
        root->appendChildNode(node);
        entries_.emplace(page.id, Entry{node, version, page.bounds});
    }
}

} // namespace peerchalk
