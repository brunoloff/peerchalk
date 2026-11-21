// Page boards on BoardCanvas: strokes and text boxes belong to pages and are kept in page
// coordinates, so the canvas moves them between the board and the page they are on.
#include "ui/board_canvas.hpp"

#include "board/board_pages.hpp"
#include "ui/conversions.hpp"
#include "ui/scene_nodes.hpp"

#include <QSGTransformNode>

#include <algorithm>
#include <cmath>

namespace peerchalk {
namespace {

Stroke moved(const Stroke &stroke, float dx, float dy) {
    Stroke result(stroke.argb(), stroke.width(), stroke.kind());
    for (const StrokePoint &point : stroke.points())
        result.addPoint({point.x + dx, point.y + dy, point.pressure});
    return result;
}

Bounds grown(const Bounds &area, float dx, float dy) {
    return {area.left - dx, area.top - dy, area.right + dx, area.bottom + dy};
}

constexpr float ruleSpacing = 8.0F * unitsPerMillimetre;
constexpr float gridSpacing = 5.0F * unitsPerMillimetre;
constexpr float minPatternPixels = 5.0F; // denser patterns are left out
constexpr float dotPixels = 2.0F;

// A page's printed pattern, in board coordinates, within the visible part of the page.
struct Pattern {
    std::vector<Bounds> lines;
    std::vector<Point> dots;

    void add(const PlacedPage &page, const Bounds &visible, float scale) {
        const float spacing = page.background == PageBackground::Ruled ? ruleSpacing : gridSpacing;
        if (page.background == PageBackground::Blank || spacing * scale < minPatternPixels)
            return;
        const Bounds &b = page.bounds;
        const float line = 1.0F / scale;
        const float top = std::max(b.top + spacing, visible.top);
        const float left = std::max(b.left + spacing, visible.left);
        const auto first = [](float from, float origin, float step) {
            return origin + std::ceil((from - origin) / step) * step;
        };
        for (float y = first(top, b.top, spacing); y < std::min(b.bottom, visible.bottom);
             y += spacing) {
            if (page.background == PageBackground::Dotted) {
                for (float x = first(left, b.left, spacing); x < std::min(b.right, visible.right);
                     x += spacing)
                    dots.push_back({x, y});
            } else {
                lines.push_back({b.left, y, b.right, y + line});
            }
        }
        if (page.background != PageBackground::Squared)
            return;
        for (float x = first(left, b.left, spacing); x < std::min(b.right, visible.right);
             x += spacing)
            lines.push_back({x, b.top, x + line, b.bottom});
    }
};

} // namespace

void BoardCanvas::refreshPages() {
    const bool wasPageBoard = pageBoard_;
    pageBoard_ = board().pages().kind() == BoardKind::Pages;
    pageMap_.refresh(board().pages());
    startViewPending_ = startViewPending_ || (pageBoard_ && !wasPageBoard);
    showStartOfPages();
}

// A page board opens on the top of its first column, at 100 % when the column fits.
void BoardCanvas::showStartOfPages() {
    constexpr float marginPx = 24.0F;
    if (!startViewPending_ || width() <= 2.0 * marginPx || pageMap_.layout().columns.empty())
        return;
    startViewPending_ = false;
    const Bounds column = pageMap_.layout().columns.front().bounds;
    const float w = static_cast<float>(width());
    const float scale = std::min(1.0F, (w - 2.0F * marginPx) / column.width());
    viewport_ = Viewport{};
    viewport_.zoomAt({0.0F, 0.0F}, scale);
    viewport_.panBy(w / 2.0F - (column.left + column.width() / 2.0F) * scale, marginPx);
    viewUpdated();
}

PageView BoardCanvas::pageView() const {
    const Bounds visible =
        viewport_.visibleWorld(static_cast<float>(width()), static_cast<float>(height()));
    const float w = visible.width();
    const float h = visible.height();
    return {grown(visible, w, h), grown(visible, 2.0F * w, 2.0F * h), viewport_.scale()};
}

std::optional<Bounds> BoardCanvas::shownBounds() const {
    return pageBoard_ ? pageMap_.bounds() : board().contentBounds();
}

void BoardCanvas::commitStroke(const Stroke &stroke) {
    if (!pageBoard_) {
        editableBoard().addStroke(stroke);
        return;
    }
    // A stroke belongs to the page it starts on; one that starts between pages is dropped.
    const StrokePoint &first = stroke.points().front();
    if (const std::optional<PlacedPage> page = pageMap_.pageAt({first.x, first.y}))
        editableBoard().pages().addStroke(page->id,
                                          moved(stroke, -page->bounds.left, -page->bounds.top));
}

std::size_t BoardCanvas::eraseAt(Point world, float radius) {
    if (!pageBoard_)
        return editableBoard().eraseNear(world, radius);
    std::size_t removed = 0;
    for (const PlacedPage &page :
         pageMap_.pagesIn(grown({world.x, world.y, world.x, world.y}, radius, radius))) {
        const Point local{world.x - page.bounds.left, world.y - page.bounds.top};
        removed += editableBoard().pages().eraseNear(page.id, local, radius);
        std::vector<std::string> hits;
        for (const TextBox &box : board().textBoxes()) {
            if (box.page == page.id && board().textBoxBounds(box).contains(local, radius))
                hits.push_back(box.id);
        }
        for (const std::string &id : hits)
            editableBoard().removeTextBox(id);
        removed += hits.size();
    }
    return removed;
}

std::vector<TextBox> BoardCanvas::placedTextBoxes() const {
    std::vector<TextBox> boxes;
    for (const TextBox &box : board().textBoxes()) {
        if (box.page.empty()) {
            boxes.push_back(box);
        } else if (const std::optional<PlacedPage> page = pageMap_.page(box.page)) {
            TextBox &placed = boxes.emplace_back(box);
            placed.x += page->bounds.left;
            placed.y += page->bounds.top;
        }
    }
    return boxes;
}

Point BoardCanvas::boxOrigin(const TextBox &box) const {
    const std::optional<PlacedPage> page =
        box.page.empty() ? std::nullopt : pageMap_.page(box.page);
    return page ? Point{page->bounds.left, page->bounds.top} : Point{0.0F, 0.0F};
}

std::optional<TextBox> BoardCanvas::newTextBoxAt(Point world) const {
    TextBox box;
    box.x = world.x;
    box.y = world.y;
    if (!pageBoard_)
        return box;
    const std::optional<PlacedPage> page = pageMap_.pageAt(world);
    if (!page)
        return std::nullopt; // text goes on pages
    box.page = page->id;
    box.x -= page->bounds.left;
    box.y -= page->bounds.top;
    return box;
}

void BoardCanvas::appendPageSheets(QSGNode *parent) const {
    const Bounds visible =
        viewport_.visibleWorld(static_cast<float>(width()), static_cast<float>(height()));
    std::vector<Bounds> sheets;
    std::vector<Bounds> edges;
    Pattern pattern;
    const float edge = 1.0F / viewport_.scale();
    for (const PlacedPage &page : pageMap_.pagesIn(visible)) {
        sheets.push_back(page.bounds);
        edges.push_back(grown(page.bounds, edge, edge));
        pattern.add(page, visible, viewport_.scale());
    }
    if (sheets.empty())
        return;
    auto *transform = new QSGTransformNode;
    transform->setMatrix(viewMatrix());
    transform->appendChildNode(createRectsNode(edges, gridColor_));
    transform->appendChildNode(createRectsNode(sheets, pageColor_));
    QColor rule = gridColor_;
    rule.setAlphaF(rule.alphaF() * 0.6F);
    if (!pattern.lines.empty())
        transform->appendChildNode(createRectsNode(pattern.lines, rule));
    if (!pattern.dots.empty())
        transform->appendChildNode(
            createDotsNode(pattern.dots, dotPixels / viewport_.scale(), gridColor_));
    parent->appendChildNode(transform);
}

} // namespace peerchalk
