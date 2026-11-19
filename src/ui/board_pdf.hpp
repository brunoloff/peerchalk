#pragma once

#include "board/board.hpp"
#include "math/math_engine.hpp"

#include <QByteArray>
#include <QHash>
#include <QString>

namespace peerchalk {

// The page margin of exported boards.
inline constexpr qreal pdfMarginCm = 3.0;

// Which formula a text box needs: its source, style and colour.
[[nodiscard]] QString pdfFormulaKey(const RichUnit &equation, std::uint32_t argb);
// The keys of every formula on the board.
[[nodiscard]] QStringList pdfFormulaKeys(const Board &board);

struct BoardPdfInput {
    const Board *board{nullptr};
    QHash<QString, MathFormula> formulas; // by pdfFormulaKey; missing ones show their source
    QString fontFamily;                   // as the text boxes on screen use
    QString title;
};

// One page holding everything on the board, at its size (1 board unit = 1/96 inch) with
// pdfMarginCm around it, scaled down only beyond PDF's 200-inch page limit. Strokes, then text
// boxes with their equations, are drawn as vectors. Throws std::invalid_argument for an empty
// board and std::runtime_error if the PDF cannot be written.
[[nodiscard]] QByteArray renderBoardPdf(const BoardPdfInput &input);

} // namespace peerchalk
