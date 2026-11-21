#pragma once

// What the PDF export's source files share (see board_pdf.hpp): drawing strokes and text
// boxes with QPainter, in board units at 96 per inch.
#include "ui/board_pdf.hpp"

#include <QByteArray>
#include <QTextDocument>

#include <memory>
#include <vector>

class QPainter;

namespace peerchalk {

class PdfEquations;

namespace pdf_drawing {

inline constexpr qreal pixelsPerInch = 96.0;
inline constexpr qreal pointsPerPixel = 72.0 / pixelsPerInch;

// A text box laid out as TextEdit lays it out on screen.
struct LaidOutBox {
    LaidOutBox(const TextBox *laid, std::unique_ptr<PdfEquations> equationHandler,
               std::unique_ptr<QTextDocument> laidDocument);
    LaidOutBox(LaidOutBox &&) noexcept;
    ~LaidOutBox();

    const TextBox *box;
    std::unique_ptr<PdfEquations> equations;
    std::unique_ptr<QTextDocument> document;
};

[[nodiscard]] LaidOutBox layOut(const BoardPdfInput &input, const TextBox &box);
void drawHighlighters(QPainter &painter, const std::vector<Stroke> &strokes);
void drawPens(QPainter &painter, const std::vector<Stroke> &strokes);
void drawTextBoxes(QPainter &painter, const std::vector<LaidOutBox> &boxes);

// A page board's PDF (board_pdf_pages.cpp).
[[nodiscard]] QByteArray renderPagesPdf(const BoardPdfInput &input);

} // namespace pdf_drawing
} // namespace peerchalk
