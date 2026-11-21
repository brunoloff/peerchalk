// Measures a large page board, 500 pages of 200 strokes each, and a 300-page PDF. Not a
// test: it prints timings for docs/page-boards.md and never fails. Run it from the build
// directory:
//   QT_QPA_PLATFORM=offscreen tests/peerchalk_page_bench
#include "board/board_pages.hpp"
#include "board/page_map.hpp"
#include "pdf/pdf_document.hpp"
#include "ui/page_scene.hpp"

#include <QBuffer>
#include <QGuiApplication>
#include <QPainter>
#include <QPdfWriter>
#include <QSGNode>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <vector>

namespace {

using namespace peerchalk;
using Clock = std::chrono::steady_clock;

constexpr int pageCount = 500;
constexpr int strokesPerPage = 200;
constexpr int pointsPerStroke = 30;

double millisecondsOf(const std::function<void()> &work) {
    const auto start = Clock::now();
    work();
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

Stroke scribble(std::mt19937 &random) {
    std::uniform_real_distribution<float> place(20.0F, 760.0F);
    std::uniform_real_distribution<float> step(-6.0F, 6.0F);
    Stroke stroke(0xFF1A1A1AU, 2.0F);
    float x = place(random);
    float y = place(random) * 1.4F;
    for (int i = 0; i < pointsPerStroke; ++i) {
        stroke.addPoint({x, y, 1.0F});
        x += 4.0F + step(random);
        y += step(random);
    }
    return stroke;
}

void fill(Board &board) {
    board.pages().start(paperA4, "Bench");
    const std::string column = board.pages().columns()[0].id;
    for (int i = 1; i < pageCount; ++i)
        board.pages().insertItem(column, {}, ColumnItemKind::Page);
    std::mt19937 random(7);
    for (const ColumnItem &page : board.pages().columns()[0].items) {
        for (int s = 0; s < strokesPerPage; ++s)
            board.pages().addStroke(page.id, scribble(random));
    }
}

// Pans down the whole column at the given zoom, one frame per step, and prints frame times.
void pan(const Board &board, const PageMap &map, float scale, const char *label) {
    const float viewWidth = 1920.0F / scale;
    const float viewHeight = 1080.0F / scale;
    const Bounds all = *map.bounds();
    QSGNode pens;
    QSGNode highlighters;
    PageStrokeNodes penNodes;
    PageStrokeNodes highlighterNodes;
    std::vector<double> frames;
    for (float top = all.top; top < all.bottom; top += 160.0F / scale) {
        const Bounds visible{0.0F, top, viewWidth, top + viewHeight};
        const PageView view{{visible.left - viewWidth, visible.top - viewHeight,
                             visible.right + viewWidth, visible.bottom + viewHeight},
                            {visible.left - 2 * viewWidth, visible.top - 2 * viewHeight,
                             visible.right + 2 * viewWidth, visible.bottom + 2 * viewHeight},
                            scale};
        frames.push_back(millisecondsOf([&] {
            penNodes.update(&pens, board, map, view, StrokeSelection::Pens);
            highlighterNodes.update(&highlighters, board, map, view,
                                    StrokeSelection::HighlightersOpaque);
        }));
    }
    std::ranges::sort(frames);
    const auto at = [&](double share) {
        return frames[std::min(frames.size() - 1, std::size_t(share * double(frames.size())))];
    };
    std::printf("%-22s %5zu frames  p50 %7.2f ms  p95 %7.2f ms  max %7.2f ms  nodes %zu\n", label,
                frames.size(), at(0.5), at(0.95), frames.back(), penNodes.size());
}

// A 300-page A4 PDF of text and figures, like a long paper.
Bytes longPdf() {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QPdfWriter writer(&buffer);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(72);
    QPainter painter(&writer);
    painter.setFont(QFont(QStringLiteral("Serif"), 10));
    for (int page = 0; page < 300; ++page) {
        if (page > 0)
            writer.newPage();
        for (int line = 0; line < 55; ++line)
            painter.drawText(60, 70 + line * 13,
                             QStringLiteral("%1.%2 Lorem ipsum dolor sit amet, the sparse Fourier "
                                            "transform of a signal with k large coefficients")
                                 .arg(page + 1)
                                 .arg(line));
        painter.drawEllipse(QPointF(400, 650), 60.0 + page % 7, 40.0);
    }
    painter.end();
    const auto *begin = reinterpret_cast<const std::byte *>(bytes.constData());
    return {begin, begin + bytes.size()};
}

void pdfTimings() {
    const auto pdf = std::make_shared<const Bytes>(longPdf());
    std::printf("300-page PDF: %.1f MB\n", double(pdf->size()) / 1e6);
    std::unique_ptr<PdfDocument> document;
    std::printf("open and read 300 page sizes (import): %.0f ms\n", millisecondsOf([&] {
                    document = PdfDocument::open(pdf).document;
                    for (int i = 0; i < document->pageCount(); ++i)
                        (void)document->pageSize(i);
                }));
    std::printf("first page at 1024 px (100 %% on screen): %.0f ms\n",
                millisecondsOf([&] { (void)document->render(0, 724, 1024); }));
    std::printf("a page at 4096 px (the sharpest whole page): %.0f ms\n",
                millisecondsOf([&] { (void)document->render(150, 2896, 4096); }));
    std::printf("six pages at 362 px (25 %%): %.0f ms\n", millisecondsOf([&] {
                    for (int i = 0; i < 6; ++i)
                        (void)document->render(200 + i, 256, 362);
                }));
}

} // namespace

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    Board board;
    std::printf("build %d pages x %d strokes: %.0f ms\n", pageCount, strokesPerPage,
                millisecondsOf([&] { fill(board); }));
    const Bytes saved = board.encodeState();
    std::printf("saved size: %.1f MB\n", double(saved.size()) / 1e6);
    Board loaded;
    std::printf("load: %.0f ms\n", millisecondsOf([&] { (void)loaded.apply(saved); }));
    PageMap map;
    std::printf("layout: %.2f ms\n", millisecondsOf([&] { map.refresh(loaded.pages()); }));
    pan(loaded, map, 1.0F, "pan at 100% (cold)");
    pan(loaded, map, 1.0F, "pan at 100% (decoded)");
    pan(loaded, map, 0.25F, "pan at 25%");
    pan(loaded, map, 0.02F, "pan at 2%");
    pdfTimings();
    return 0;
}
