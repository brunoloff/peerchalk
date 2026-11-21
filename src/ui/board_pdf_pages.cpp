// A page board's PDF: one page per board page, at its exact size (see renderBoardPdf).
#include "ui/board_pdf_drawing.hpp"

#include "board/board_pages.hpp"
#include "pdf/pdf_compose.hpp"
#include "pdf/pdf_outline.hpp"

#include <QBuffer>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

#include <map>
#include <stdexcept>

namespace peerchalk::pdf_drawing {
namespace {

struct ExportPage {
    std::string id;
    PaperSize size;
    std::optional<PdfSource> pdf;
    PageBackground background;
};

constexpr qreal ruleSpacing = 8.0 * 96.0 / 25.4; // as on screen: 8 mm rules, a 5 mm grid
constexpr qreal gridSpacing = 5.0 * 96.0 / 25.4;
const QColor patternColor(0xD6, 0xD2, 0xC9, 0x99);

// The page's printed pattern, as the board shows it.
void drawBackground(QPainter &painter, const ExportPage &page) {
    const qreal w = page.size.width;
    const qreal h = page.size.height;
    const qreal step = page.background == PageBackground::Ruled ? ruleSpacing : gridSpacing;
    if (page.background == PageBackground::Blank || page.pdf)
        return;
    painter.save();
    QPen pen(patternColor);
    pen.setWidthF(page.background == PageBackground::Dotted ? 2.0 : 1.0);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    for (qreal y = step; y < h; y += step) {
        if (page.background != PageBackground::Dotted) {
            painter.drawLine(QPointF(0.0, y), QPointF(w, y));
            continue;
        }
        for (qreal x = step; x < w; x += step)
            painter.drawPoint(QPointF(x, y));
    }
    if (page.background == PageBackground::Squared) {
        for (qreal x = step; x < w; x += step)
            painter.drawLine(QPointF(x, 0.0), QPointF(x, h));
    }
    painter.restore();
}

std::vector<ExportPage> exportPages(const BoardPages &pages) {
    std::vector<ExportPage> list;
    for (const ColumnSpec &column : pages.columns()) {
        for (const ColumnItem &item : column.items) {
            if (item.kind == ColumnItemKind::Page)
                list.push_back(
                    {item.id, item.size.value_or(column.paper), item.pdf, column.background});
        }
    }
    return list;
}

// Each column's heading leads to its first page, and each subheading to the page after it
// (or, at the end of a column, the page before it).
std::vector<PdfOutlineEntry> outlineOf(const BoardPages &pages) {
    std::vector<PdfOutlineEntry> entries;
    int page = 0;
    for (const ColumnSpec &column : pages.columns()) {
        const int first = page;
        std::vector<std::string> waiting; // subheadings until the next page
        for (const ColumnItem &item : column.items) {
            if (item.kind == ColumnItemKind::Subheading) {
                waiting.push_back(item.text);
                continue;
            }
            if (page == first)
                entries.push_back(
                    {column.title.empty() ? "Untitled column" : column.title, 1, page});
            for (const std::string &title : waiting)
                entries.push_back({title, 2, page});
            waiting.clear();
            ++page;
        }
        for (const std::string &title : waiting) {
            if (page > first)
                entries.push_back({title, 2, page - 1});
        }
    }
    return entries;
}

QPageSize exactSize(PaperSize size) {
    return QPageSize(QSizeF(size.width * pointsPerPixel, size.height * pointsPerPixel),
                     QPageSize::Point, QString(), QPageSize::ExactMatch);
}

// Every page's ink, each page at its size; the PDFs under them are added afterwards.
QByteArray drawPages(const BoardPdfInput &input, const std::vector<ExportPage> &pages) {
    std::map<std::string, std::vector<LaidOutBox>> boxes;
    for (const TextBox &box : input.board->textBoxes()) {
        if (!box.page.empty())
            boxes[box.page].push_back(layOut(input, box));
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QPdfWriter writer(&buffer);
    writer.setResolution(int(pixelsPerInch));
    writer.setPageSize(exactSize(pages.front().size));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setTitle(input.title);
    writer.setCreator(QStringLiteral("PeerChalk"));
    QPainter painter;
    if (!painter.begin(&writer))
        throw std::runtime_error("The PDF could not be written");
    painter.setRenderHint(QPainter::Antialiasing);
    for (std::size_t i = 0; i < pages.size(); ++i) {
        if (i > 0) {
            writer.setPageSize(exactSize(pages[i].size));
            writer.newPage();
        }
        const std::vector<Stroke> &strokes = input.board->pages().strokes(pages[i].id);
        painter.save();
        painter.setClipRect(QRectF(0.0, 0.0, pages[i].size.width, pages[i].size.height));
        drawBackground(painter, pages[i]);
        drawHighlighters(painter, strokes);
        drawPens(painter, strokes);
        drawTextBoxes(painter, boxes[pages[i].id]);
        painter.restore();
    }
    painter.end();
    return bytes;
}

} // namespace

QByteArray renderPagesPdf(const BoardPdfInput &input) {
    const std::vector<ExportPage> pages = exportPages(input.board->pages());
    if (pages.empty())
        throw std::invalid_argument("The board has no pages");
    const QByteArray ink = drawPages(input, pages);
    // The PDF writer rounds page sizes to whole points; PDFium sets them exactly.
    std::vector<PdfComposedPage> composed;
    for (const ExportPage &page : pages) {
        PdfComposedPage &out = composed.emplace_back();
        out.widthPoints = static_cast<float>(page.size.width * pointsPerPixel);
        out.heightPoints = static_cast<float>(page.size.height * pointsPerPixel);
        if (!page.pdf)
            continue;
        std::shared_ptr<const Bytes> pdf = input.pdfBlob ? input.pdfBlob(page.pdf->blob) : nullptr;
        if (!pdf)
            throw std::runtime_error("A PDF on this board has not reached this device yet");
        out.under = PdfUnderlay{std::move(pdf), page.pdf->page};
    }
    const auto *begin = reinterpret_cast<const std::byte *>(ink.constData());
    const Bytes combined = addPdfOutline(composePdf(Bytes(begin, begin + ink.size()), composed),
                                         outlineOf(input.board->pages()));
    return {reinterpret_cast<const char *>(combined.data()), qsizetype(combined.size())};
}

} // namespace peerchalk::pdf_drawing
