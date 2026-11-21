// PDF pages on BoardCanvas: asking the renderer for the pages in view, and drawing what it
// has. Pages shown taller than a whole render allows get sharp tiles of their visible part
// over the whole page.
#include "ui/board_canvas.hpp"

#include "ui/conversions.hpp"
#include "ui/pdf_renderer.hpp"
#include "ui/pdf_textures.hpp"

#include <QQuickWindow>
#include <QSGImageNode>

#include <algorithm>
#include <cmath>

namespace peerchalk {

using Tile = PdfRenderer::Tile;

// How many pixels tall the page is on screen.
double BoardCanvas::pdfPixelsFor(const Bounds &page) const {
    const qreal ratio = window() != nullptr ? window()->effectiveDevicePixelRatio() : 1.0;
    return page.height() * viewport_.scale() * ratio;
}

int BoardCanvas::pdfHeightFor(const Bounds &page) const {
    return PdfRenderer::bucketFor(pdfPixelsFor(page));
}

void BoardCanvas::requestPdfPages() {
    PdfRenderer *pdfs = document_ ? document_->pdfRenderer() : nullptr;
    if (pdfs == nullptr || !pageBoard_)
        return;
    const Bounds visible =
        viewport_.visibleWorld(static_cast<float>(width()), static_cast<float>(height()));
    const Point centre{(visible.left + visible.right) / 2.0F,
                       (visible.top + visible.bottom) / 2.0F};
    std::vector<std::pair<float, PdfRenderer::Request>> wanted;
    for (const PlacedPage &page : pageMap_.pagesIn(
             {visible.left - visible.width() / 2.0F, visible.top - visible.height() / 2.0F,
              visible.right + visible.width() / 2.0F, visible.bottom + visible.height() / 2.0F})) {
        if (!page.pdf)
            continue;
        const float dx = (page.bounds.left + page.bounds.right) / 2.0F - centre.x;
        const float dy = (page.bounds.top + page.bounds.bottom) / 2.0F - centre.y;
        wanted.push_back({dx * dx + dy * dy, {*page.pdf, pdfHeightFor(page.bounds)}});
        const double pixels = pdfPixelsFor(page.bounds);
        if (!PdfRenderer::needsTiles(pixels))
            continue;
        const int tiled = PdfRenderer::tiledHeightFor(pixels);
        for (const Tile &tile :
             PdfRenderer::tilesIn(page.bounds, visible, tiled)) // after the whole
            wanted.push_back({dx * dx + dy * dy + 1.0F, {*page.pdf, tiled, tile.index}});
    }
    std::ranges::stable_sort(wanted, {}, &std::pair<float, PdfRenderer::Request>::first);
    std::vector<PdfRenderer::Request> requests;
    for (auto &[distance, request] : wanted)
        requests.push_back(std::move(request));
    pdfs->want(std::move(requests));
}

void BoardCanvas::appendImage(QSGNode *parent, PdfTextureNode &textures, const QImage &image,
                              const Bounds &world) const {
    QSGTexture *texture = image.isNull() ? nullptr : textures.texture(*window(), image);
    if (texture == nullptr)
        return;
    QSGImageNode *node = window()->createImageNode();
    node->setTexture(texture);
    node->setOwnsTexture(false);
    node->setFiltering(QSGTexture::Linear);
    node->setRect(toRect(world));
    parent->appendChildNode(node);
}

void BoardCanvas::appendPdfImages(QSGNode *parent, PdfTextureNode &textures,
                                  const std::vector<PlacedPage> &pages) const {
    PdfRenderer *pdfs = document_ ? document_->pdfRenderer() : nullptr;
    if (pdfs == nullptr || window() == nullptr)
        return;
    const Bounds visible =
        viewport_.visibleWorld(static_cast<float>(width()), static_cast<float>(height()));
    for (const PlacedPage &page : pages) {
        if (!page.pdf)
            continue;
        appendImage(parent, textures, pdfs->best(*page.pdf, pdfHeightFor(page.bounds)),
                    page.bounds);
        const double pixels = pdfPixelsFor(page.bounds);
        if (!PdfRenderer::needsTiles(pixels))
            continue;
        const int tiled = PdfRenderer::tiledHeightFor(pixels);
        for (const Tile &tile : PdfRenderer::tilesIn(page.bounds, visible, tiled))
            appendImage(parent, textures, pdfs->tile(*page.pdf, tiled, tile.index), tile.world);
    }
}

} // namespace peerchalk
