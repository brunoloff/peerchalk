#include "ui/board_exporter.hpp"
#include "ui/board_pdf.hpp"

#include <QRegularExpression>
#include <QSizeF>

#include <gtest/gtest.h>

#include <stdexcept>

namespace peerchalk {
namespace {

constexpr double marginPoints = 3.0 / 2.54 * 72.0;

Stroke line(float x1, float y1, float x2, float y2) {
    Stroke stroke(0xFF000000U, 4.0F);
    stroke.addPoint({x1, y1, 1.0F});
    stroke.addPoint({x2, y2, 1.0F});
    return stroke;
}

QSizeF mediaBox(const QByteArray &pdf) {
    const QRegularExpression box(QStringLiteral(R"(/MediaBox \[0 0 ([0-9.]+) ([0-9.]+)\])"));
    const QRegularExpressionMatch match = box.match(QString::fromLatin1(pdf));
    return match.hasMatch() ? QSizeF(match.captured(1).toDouble(), match.captured(2).toDouble())
                            : QSizeF();
}

BoardPdfInput inputFor(const Board &board) { return {&board, {}, QStringLiteral("Sans"), {}}; }

TextBox boxWith(RichText body) {
    TextBox box;
    box.id = "box";
    box.body = std::move(body);
    return box;
}

TEST(BoardPdf, PageIsTheContentPlusThreeCentimetreMargins) {
    Board board;
    board.addStroke(line(0, 0, 200, 100));
    const QByteArray pdf = renderBoardPdf(inputFor(board));
    ASSERT_TRUE(pdf.startsWith("%PDF"));
    const Bounds content = *board.contentBounds();
    const QSizeF page = mediaBox(pdf);
    EXPECT_NEAR(page.width(), content.width() * 0.75 + 2 * marginPoints, 1.0);
    EXPECT_NEAR(page.height(), content.height() * 0.75 + 2 * marginPoints, 1.0);
}

TEST(BoardPdf, HugeBoardsShrinkToTheLargestPage) {
    Board board;
    board.addStroke(line(0, 0, 100000, 10));
    const QSizeF page = mediaBox(renderBoardPdf(inputFor(board)));
    EXPECT_NEAR(page.width(), 14400.0, 1.0);
    EXPECT_GT(page.height(), 2 * marginPoints);
}

TEST(BoardPdf, EmptyBoardsAreRefused) {
    Board board;
    EXPECT_THROW((void)renderBoardPdf(inputFor(board)), std::invalid_argument);
    EXPECT_THROW((void)renderBoardPdf({}), std::invalid_argument);
}

TEST(BoardPdf, TextBoxesCountTowardsThePage) {
    Board board;
    TextBox box = boxWith(plainRichText(u"hello"));
    box.x = 0.0F;
    box.width = 400.0F;
    board.addTextBox(box);
    const QSizeF page = mediaBox(renderBoardPdf(inputFor(board)));
    EXPECT_NEAR(page.width(), 400.0 * 0.75 + 2 * marginPoints, 1.0);
}

TEST(BoardPdf, EquationsAreDrawnFromTheirFormulas) {
    Board board;
    RichText body = plainRichText(u"x ");
    body.push_back(equationUnit("\\sum_i i^2", true));
    board.addTextBox(boxWith(body));
    const QStringList keys = pdfFormulaKeys(board);
    ASSERT_EQ(keys.size(), 1);
    EXPECT_TRUE(keys.front().endsWith(QStringLiteral("|1|\\sum_i i^2")));
    const QByteArray withoutFormula = renderBoardPdf(inputFor(board));
    MathFormula formula;
    formula.vertices = {0, 0, 1, 0, 0, 1};
    formula.indices = {0, 1, 2};
    formula.runs = {{0, 3, 0x000000ffU}};
    formula.width = formula.height = 1.0;
    BoardPdfInput input = inputFor(board);
    input.formulas.insert(keys.front(), formula);
    EXPECT_NE(renderBoardPdf(input), withoutFormula);
}

TEST(BoardExporter, FileNamesAvoidCharactersSystemsReject) {
    EXPECT_EQ(BoardExporter::fileName(QStringLiteral("Week 3: a/b")), QStringLiteral("Week 3 a b.pdf"));
    EXPECT_EQ(BoardExporter::fileName(QStringLiteral("  ")), QStringLiteral("Board.pdf"));
}

} // namespace
} // namespace peerchalk
