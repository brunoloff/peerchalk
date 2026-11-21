#include "board/board_pages.hpp"
#include "pdf/pdf_document.hpp"
#include "test_pdf.hpp"
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

std::unique_ptr<PdfDocument> openPdf(const QByteArray &pdf) {
    const auto *begin = reinterpret_cast<const std::byte *>(pdf.constData());
    return PdfDocument::open(std::make_shared<const Bytes>(begin, begin + pdf.size())).document;
}

// The blue channel of a pixel of a rendered page: 0 where there is black ink.
int blueAt(const PdfImage &image, int x, int y) {
    return image.pixels[(std::size_t(y) * std::size_t(image.width) + std::size_t(x)) * 4];
}

TEST(BoardPdf, PageBoardsBecomeOnePageEachAtItsExactSize) {
    Board board;
    board.pages().start(paperA4, "Notes");
    const std::string first = board.pages().columns()[0].items[0].id;
    board.pages().addColumn({}, landscape(paperLetter), "Wide");
    board.pages().addStroke(first, line(100.0F, 100.0F, 300.0F, 100.0F));
    const std::unique_ptr<PdfDocument> pdf = openPdf(renderBoardPdf(inputFor(board)));
    ASSERT_TRUE(pdf);
    ASSERT_EQ(pdf->pageCount(), 2);
    EXPECT_NEAR(pdf->pageSize(0).width, paperA4.width, 0.05F);
    EXPECT_NEAR(pdf->pageSize(0).height, paperA4.height, 0.05F);
    EXPECT_NEAR(pdf->pageSize(1).width, paperLetter.height, 0.05F);
    const PaperSize a4 = pdf->pageSize(0);
    const PdfImage page = pdf->render(0, int(a4.width), int(a4.height)); // 1 pixel per unit
    EXPECT_LT(blueAt(page, 200, 100), 64); // the stroke, where it is on the page
    EXPECT_EQ(blueAt(page, 200, 300), 0xFF);
}

TEST(BoardPdf, PdfPagesKeepTheirPdfUnderTheInk) {
    Board board;
    board.pages().start(paperA4, "Notes");
    const auto sample = std::make_shared<const Bytes>(test::samplePdf());
    board.pages().addPdfColumn({}, "Paper", "sample", {paperA4, landscape(paperLetter)});
    const std::string pdfPage = board.pages().columns()[1].items[0].id;
    board.pages().addStroke(pdfPage, line(600.0F, 100.0F, 700.0F, 100.0F));
    const std::string turnedPage = board.pages().columns()[1].items[1].id;
    board.pages().addStroke(turnedPage, line(900.0F, 700.0F, 1000.0F, 700.0F));
    BoardPdfInput input = inputFor(board);
    EXPECT_THROW((void)renderBoardPdf(input), std::runtime_error); // the PDF is not here
    input.pdfBlob = [&](const std::string &hash) { return hash == "sample" ? sample : nullptr; };
    const std::unique_ptr<PdfDocument> pdf = openPdf(renderBoardPdf(input));
    ASSERT_TRUE(pdf);
    ASSERT_EQ(pdf->pageCount(), 3);
    EXPECT_NEAR(pdf->pageSize(2).width, paperLetter.height, 0.05F); // still turned
    const PaperSize size = pdf->pageSize(1);
    const PdfImage page = pdf->render(1, int(size.width), int(size.height));
    EXPECT_EQ(blueAt(page, 5, int(size.height) - 5), 0); // the PDF's own black square
    EXPECT_LT(blueAt(page, 650, 100), 64);               // and the ink over it
    const PaperSize turned = pdf->pageSize(2);           // a page shown a quarter turn round
    const PdfImage turnedImage = pdf->render(2, int(turned.width), int(turned.height));
    EXPECT_LT(blueAt(turnedImage, 950, 700), 64); // its ink sits as it was drawn
    EXPECT_EQ(blueAt(turnedImage, 950, 300), 0xFF);
}

TEST(BoardExporter, FileNamesAvoidCharactersSystemsReject) {
    EXPECT_EQ(BoardExporter::fileName(QStringLiteral("Week 3: a/b")),
              QStringLiteral("Week 3 a b.pdf"));
    EXPECT_EQ(BoardExporter::fileName(QStringLiteral("  ")), QStringLiteral("Board.pdf"));
}

} // namespace
} // namespace peerchalk
