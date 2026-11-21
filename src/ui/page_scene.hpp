#pragma once

#include "board/board.hpp"
#include "board/page_map.hpp"
#include "ui/scene_nodes.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

class QSGNode;

namespace peerchalk {

// Where a page board is seen: what is on screen, how much around it gets nodes ahead of
// time, and how far away nodes are let go. All in board coordinates.
struct PageView {
    Bounds show; // pages here get nodes
    Bounds keep; // pages outside are released; contains `show`
    float scale; // screen pixels per board unit
};

// A page board's strokes as scene nodes: per page in view, a node moved to the page and
// clipped to it, holding the page's strokes merged into one mesh. Nodes stay between frames
// and are rebuilt only when their page's strokes change or the page moves.
class PageStrokeNodes {
  public:
    // Pages shorter than this on screen show no strokes.
    static constexpr float minPagePixels = 12.0F;

    // Brings root's children up to date; root holds only nodes this object made.
    void update(QSGNode *root, const Board &board, const PageMap &map, const PageView &view,
                StrokeSelection selection);
    // Forgets the nodes, for when root was deleted or emptied elsewhere.
    void reset() { entries_.clear(); }
    [[nodiscard]] std::size_t size() const { return entries_.size(); }

  private:
    struct Entry {
        QSGNode *node;
        std::uint64_t version;
        Bounds bounds;
    };

    void release(QSGNode *root, const Board &board, const PageMap &map, const PageView &view);

    std::unordered_map<std::string, Entry> entries_;
};

} // namespace peerchalk
