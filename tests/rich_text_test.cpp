#include "ui/board_canvas.hpp"
#include "ui/equation_syntax.hpp"
#include "ui/rich_text_binding.hpp"
#include "ui/rich_text_document.hpp"

#include <QAbstractTextDocumentLayout>
#include <QSignalSpy>
#include <QTest>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

constexpr int waitMs = 5000;

std::u16string characters(const RichText &body) {
    std::u16string out;
    for (const RichUnit &unit : body)
        out += unit.ch;
    return out;
}

RichText sampleText() {
    RichText body = plainRichText(u"ab\ncd");
    body[1].bold = true;
    body[3].italic = true;
    body.insert(body.begin() + 2, equationUnit("x^2", true));
    return body;
}

TEST(RichTextDocument, RoundTripsFormattingBreaksAndEquations) {
    QTextDocument document;
    applyRichText(document, sampleText());
    EXPECT_EQ(document.blockCount(), 2);
    EXPECT_EQ(readRichText(document), sampleText());
}

TEST(RichTextDocument, EditsLeaveOtherCursorsInPlace) {
    QTextDocument document;
    applyRichText(document, plainRichText(u"hello world"));
    QTextCursor cursor(&document);
    cursor.setPosition(8);
    applyRichText(document, plainRichText(u"oh hello world"));
    EXPECT_EQ(cursor.position(), 11);
    RichText bold = plainRichText(u"oh hello world");
    bold[0].bold = true;
    applyRichText(document, bold);
    EXPECT_EQ(cursor.position(), 11);
    EXPECT_EQ(readRichText(document), bold);
}

TEST(DollarEquations, ConvertInlineAndDisplayForms) {
    RichText inlineForm = plainRichText(u"a $x^2$");
    ASSERT_TRUE(convertTypedEquation(inlineForm, inlineForm.size()));
    ASSERT_EQ(inlineForm.size(), 3U);
    EXPECT_EQ(inlineForm[2].tex, "x^2");
    EXPECT_FALSE(inlineForm[2].displayMath);
    RichText displayForm = plainRichText(u"$$y$$");
    ASSERT_TRUE(convertTypedEquation(displayForm, displayForm.size()));
    ASSERT_EQ(displayForm.size(), 2U); // the cursor continues on the next line
    EXPECT_TRUE(displayForm[0].displayMath);
    EXPECT_EQ(displayForm[1].ch, u'\n');
}

TEST(DollarEquations, BlocksGetTheirOwnLine) {
    for (const std::u16string_view text : {u"a \\[ y \\]", u"a $$ y $$"}) {
        RichText body = plainRichText(text);
        const std::optional<std::size_t> cursor = convertTypedEquation(body, body.size());
        ASSERT_TRUE(cursor.has_value());
        EXPECT_EQ(characters(body), u"a \n\uFFFC\n");
        EXPECT_EQ(body[3].tex, "y");
        EXPECT_EQ(*cursor, body.size());
    }
}

TEST(DollarEquations, BlocksMaySpanLinesButEscapesDoNotOpenThem) {
    RichText lines = plainRichText(u"\\[a\nb\\]");
    ASSERT_TRUE(convertTypedEquation(lines, lines.size()).has_value());
    EXPECT_EQ(lines[0].tex, "a\nb");
    RichText escaped = plainRichText(u"\\\\[a\\]");
    EXPECT_FALSE(convertTypedEquation(escaped, escaped.size()).has_value());
}

TEST(RichTextDocument, CentersLinesHoldingOnlyABlockEquation) {
    QTextDocument document;
    RichText body = plainRichText(u"a\n");
    body.push_back(equationUnit("x", true));
    applyRichText(document, body);
    EXPECT_EQ(document.lastBlock().blockFormat().alignment(), Qt::AlignHCenter);
    EXPECT_EQ(document.firstBlock().blockFormat().alignment(), Qt::AlignLeft);
    body.push_back(plainRichText(u"b").front());
    applyRichText(document, body);
    EXPECT_EQ(document.lastBlock().blockFormat().alignment(), Qt::AlignLeft);
}

TEST(DollarEquations, LeaveOrdinaryDollarsAlone) {
    for (const std::u16string_view text :
         {u"$", u"$$", u"costs $5 and $", u"\\$x$", u"$a\nb$", u"$$x$"}) {
        RichText body = plainRichText(text);
        EXPECT_FALSE(convertTypedEquation(body, body.size()));
        EXPECT_EQ(characters(body), text);
    }
}

class RichTextBindingTest : public ::testing::Test {
  protected:
    void SetUp() override {
        canvas_.setSize({800, 600});
        canvas_.setTool(BoardCanvas::Tool::Text);
        TextBox box;
        box.id = "box";
        box.body = plainRichText(u"hello");
        board().addTextBox(box);
        binding_.setCanvas(&canvas_);
        binding_.setTextDocument(&document_);
        binding_.setBoxId(QStringLiteral("box"));
    }
    Board &board() { return canvas_.document()->board(); }
    [[nodiscard]] std::u16string boardText() {
        return characters(board().textBox("box")->body);
    }
    void type(int position, const QString &text) {
        QTextCursor cursor(&document_);
        cursor.setPosition(position);
        cursor.insertText(text);
    }
    bool waitForBoardText(std::u16string_view text) {
        return QTest::qWaitFor([&] { return boardText() == text; }, waitMs);
    }

    BoardCanvas canvas_;
    QTextDocument document_;
    RichTextBinding binding_;
};

TEST_F(RichTextBindingTest, ShowsTheBoardsText) {
    EXPECT_EQ(document_.toPlainText(), QStringLiteral("hello"));
}

TEST_F(RichTextBindingTest, TypingReachesTheBoard) {
    type(5, QStringLiteral(" you"));
    EXPECT_TRUE(waitForBoardText(u"hello you"));
}

TEST_F(RichTextBindingTest, BoardChangesReachTheDocument) {
    QSignalSpy changed(&canvas_, &BoardCanvas::textBoxesChanged);
    board().setTextBoxBody("box", plainRichText(u"hello there"));
    ASSERT_TRUE(changed.wait(waitMs));
    EXPECT_EQ(document_.toPlainText(), QStringLiteral("hello there"));
}

TEST_F(RichTextBindingTest, TypedDollarsBecomeAnEquation) {
    type(5, QStringLiteral(" $x$"));
    ASSERT_TRUE(waitForBoardText(u" ￼" == std::u16string_view() ? u"" : u"hello ￼"));
    EXPECT_EQ(board().textBox("box")->body.back().tex, "x");
    EXPECT_TRUE(QTest::qWaitFor([&] { return binding_.equations().size() == 1; }, waitMs));
}

TEST_F(RichTextBindingTest, DollarsTypedInOneBurstStillConvert) {
    // Browsers can deliver several keys before the binding gets to run.
    for (const QChar ch : QStringLiteral(" $x$ y"))
        type(document_.characterCount() - 1, QString(ch));
    ASSERT_TRUE(waitForBoardText(u"hello \uFFFC y"));
    EXPECT_EQ(board().textBox("box")->body[6].tex, "x");
}

TEST_F(RichTextBindingTest, TextMakesRoomOnceAnEquationIsMeasured) {
    binding_.insertEquation(0, QStringLiteral("\\sum_{a}^{b} xxxxxxxxxxxxxxxx"), false);
    (void)document_.documentLayout()->documentSize(); // laid out before the size is known
    const qreal unmeasured = document_.idealWidth();
    QSignalSpy reported(&document_, &QTextDocument::contentsChange); // what makes TextEdit redraw
    EXPECT_TRUE(QTest::qWaitFor([&] { return document_.idealWidth() > unmeasured + 50.0; }, waitMs));
    EXPECT_GT(reported.count(), 0);
    EXPECT_EQ(readRichText(document_), board().textBox("box")->body);
}

TEST_F(RichTextBindingTest, ArrowsStopAtAnEquationThenPassIt) {
    binding_.insertEquation(2, QStringLiteral("x"), false); // he|=|llo
    EXPECT_EQ(binding_.moveAcrossEquation(1, 1), -1);
    EXPECT_EQ(binding_.moveAcrossEquation(2, 1), 2);
    EXPECT_EQ(binding_.focusedEquation(), 2);
    EXPECT_EQ(binding_.equationReturnPosition(), 2);
    EXPECT_EQ(binding_.focusedEquationTex(), QStringLiteral("x"));
    EXPECT_EQ(binding_.moveAcrossEquation(2, 1), 3);
    EXPECT_EQ(binding_.focusedEquation(), -1);
}

TEST_F(RichTextBindingTest, ComingFromTheRightReturnsToTheRight) {
    binding_.insertEquation(2, QStringLiteral("x"), false);
    EXPECT_EQ(binding_.moveAcrossEquation(3, -1), 3);
    EXPECT_EQ(binding_.focusedEquation(), 2);
    EXPECT_EQ(binding_.equationReturnPosition(), 3);
    EXPECT_EQ(binding_.moveAcrossEquation(3, -1), 2);
    EXPECT_EQ(binding_.focusedEquation(), -1);
}

TEST_F(RichTextBindingTest, FocusFollowsEditsAndEndsWithTheEquation) {
    binding_.insertEquation(2, QStringLiteral("x"), false);
    binding_.focusEquation(2, true);
    RichText body = board().textBox("box")->body;
    body.insert(body.begin(), plainRichText(u"#").front());
    QSignalSpy focus(&binding_, &RichTextBinding::focusedEquationChanged);
    board().setTextBoxBody("box", body);
    ASSERT_TRUE(focus.wait(waitMs));
    EXPECT_EQ(binding_.focusedEquation(), 3);
    body.erase(body.begin() + 3);
    board().setTextBoxBody("box", body);
    ASSERT_TRUE(focus.wait(waitMs));
    EXPECT_EQ(binding_.focusedEquation(), -1);
}

TEST_F(RichTextBindingTest, TheFocusedEquationCanBeDeleted) {
    binding_.insertEquation(2, QStringLiteral("x"), false);
    binding_.focusEquation(2, false);
    binding_.removeFocusedEquation();
    EXPECT_EQ(boardText(), u"hello");
    EXPECT_EQ(binding_.focusedEquation(), -1);
}

TEST_F(RichTextBindingTest, OnlyEquationsCanBeFocused) {
    binding_.focusEquation(1, false);
    EXPECT_EQ(binding_.focusedEquation(), -1);
}

TEST_F(RichTextBindingTest, SelectionsAndPendingStylesBecomeBold) {
    binding_.toggleBold(0, 2);
    EXPECT_TRUE(board().textBox("box")->body[1].bold);
    EXPECT_FALSE(board().textBox("box")->body[2].bold);
    binding_.toggleBold(5, 5);
    type(5, QStringLiteral("!"));
    ASSERT_TRUE(waitForBoardText(u"hello!"));
    EXPECT_TRUE(board().textBox("box")->body[5].bold);
}

TEST_F(RichTextBindingTest, ReplacesEquationsAndRejectsOtherPositions) {
    binding_.insertEquation(0, QStringLiteral("a"), false);
    binding_.replaceEquation(0, QStringLiteral("b"), true);
    EXPECT_EQ(board().textBox("box")->body[0].tex, "b");
    EXPECT_TRUE(board().textBox("box")->body[0].displayMath);
    EXPECT_EQ(binding_.replaceEquation(2, QStringLiteral("c"), false), -1);
    EXPECT_EQ(boardText()[2], u'h'); // the block keeps a line of its own
}

TEST(BoardCanvasText, ReadOnlyBoardsRefuseChanges) {
    BoardCanvas canvas;
    TextBox box;
    box.id = "box";
    canvas.document()->board().addTextBox(box);
    canvas.setReadOnly(true);
    canvas.moveTextBox(QStringLiteral("box"), {100.0, 100.0});
    canvas.removeTextBox(QStringLiteral("box"));
    canvas.editTextBox(QStringLiteral("box"));
    ASSERT_NE(canvas.board().textBox("box"), nullptr);
    EXPECT_FLOAT_EQ(canvas.board().textBox("box")->x, 0.0F);
    EXPECT_TRUE(canvas.editingTextBox().isEmpty());
}

TEST(BoardCanvasText, FinishingRemovesAnEmptyBox) {
    BoardCanvas canvas;
    TextBox box;
    box.id = "box";
    canvas.document()->board().addTextBox(box);
    canvas.editTextBox(QStringLiteral("box"));
    EXPECT_EQ(canvas.editingTextBox(), QStringLiteral("box"));
    canvas.finishTextEditing();
    EXPECT_TRUE(canvas.editingTextBox().isEmpty());
    EXPECT_TRUE(canvas.board().textBoxes().empty());
}

TEST(BoardCanvasText, ModelFollowsTheBoard) {
    BoardCanvas canvas;
    TextBox box;
    box.id = "box";
    box.body = plainRichText(u"x");
    canvas.document()->board().addTextBox(box);
    QSignalSpy changed(&canvas, &BoardCanvas::textBoxesChanged);
    ASSERT_TRUE(changed.wait(waitMs));
    EXPECT_EQ(canvas.textBoxes()->rowCount(), 1);
    canvas.removeTextBox(QStringLiteral("box"));
    EXPECT_EQ(canvas.textBoxes()->rowCount(), 0);
}

} // namespace
} // namespace peerchalk
