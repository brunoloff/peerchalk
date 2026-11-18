#include "board/board.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace peerchalk {
namespace {

TextBox makeBox(const std::string &id = "box", std::u16string_view text = u"") {
    TextBox box;
    box.id = id;
    box.x = 10.0F;
    box.y = 20.0F;
    box.body = plainRichText(text);
    return box;
}

std::u16string characters(const RichText &body) {
    std::u16string out;
    for (const RichUnit &unit : body)
        out += unit.ch;
    return out;
}

void sync(const Board &from, Board &to) {
    ASSERT_TRUE(to.apply(from.difference(to.stateVector())));
}

TEST(RichText, DiffFindsTheSmallestReplacement) {
    const RichChange change = diffRichText(plainRichText(u"hello"), plainRichText(u"help me"));
    EXPECT_EQ(change.start, 3U);
    EXPECT_EQ(change.removed, 2U);
    EXPECT_EQ(change.added, 4U);
    EXPECT_FALSE(change.sameCharacters);
    EXPECT_TRUE(diffRichText(plainRichText(u"a"), plainRichText(u"a")).empty());
}

TEST(RichText, DiffHandlesRepeatsWithoutOverlap) {
    const RichChange change = diffRichText(plainRichText(u"aa"), plainRichText(u"aaa"));
    EXPECT_EQ(change.start, 2U);
    EXPECT_EQ(change.removed, 0U);
    EXPECT_EQ(change.added, 1U);
}

TEST(RichText, DiffRecognisesFormattingOnlyChanges) {
    RichText bold = plainRichText(u"abc");
    bold[1].bold = true;
    const RichChange change = diffRichText(plainRichText(u"abc"), bold);
    EXPECT_EQ(change.start, 1U);
    EXPECT_TRUE(change.sameCharacters);
}

TEST(RichText, DiffKeepsSurrogatePairsWhole) {
    const RichChange change = diffRichText(plainRichText(u"\U0001F600"), plainRichText(u"\U0001F601"));
    EXPECT_EQ(change.start, 0U);
    EXPECT_EQ(change.removed, 2U);
    EXPECT_EQ(change.added, 2U);
}

TEST(RichText, ConvertsBetweenEncodings) {
    const std::u16string text = u"aé€\U0001F600";
    EXPECT_EQ(toUtf16(toUtf8(text)), text);
    EXPECT_EQ(toUtf8(std::u16string(1, char16_t(0xD800))), "\xEF\xBF\xBD");
    EXPECT_EQ(toUtf16("\xC0\x80"), std::u16string(1, char16_t(0xFFFD)));
}

TEST(BoardText, StoresBoxesAndTheirText) {
    Board board;
    board.addTextBox(makeBox("box", u"héllo \U0001F600"));
    ASSERT_EQ(board.textBoxes().size(), 1U);
    const TextBox &box = board.textBoxes().front();
    EXPECT_FLOAT_EQ(box.x, 10.0F);
    EXPECT_EQ(characters(box.body), u"héllo \U0001F600");
}

TEST(BoardText, RejectsInvalidBoxes) {
    Board board;
    TextBox box = makeBox();
    box.width = 1.0F;
    EXPECT_THROW(board.addTextBox(box), std::invalid_argument);
    board.addTextBox(makeBox());
    EXPECT_THROW(board.addTextBox(makeBox()), std::invalid_argument);
    EXPECT_THROW(board.setTextBoxBody("box", RichText(maxRichTextUnits + 1)), std::invalid_argument);
}

TEST(BoardText, KeepsFormattingAndEquations) {
    Board board;
    board.addTextBox(makeBox());
    RichText body = plainRichText(u"ab");
    body[1].bold = true;
    body[1].italic = true;
    body.push_back(equationUnit("\\frac{a}{b}", true));
    body.push_back(plainRichText(u"c").front());
    board.setTextBoxBody("box", body);
    EXPECT_EQ(board.textBox("box")->body, body);
    body[1].bold = false;
    board.setTextBoxBody("box", body);
    EXPECT_EQ(board.textBox("box")->body, body);
}

TEST(BoardText, ConcurrentTypingMerges) {
    Board alice;
    Board bob;
    alice.addTextBox(makeBox("box", u"middle"));
    sync(alice, bob);
    alice.setTextBoxBody("box", plainRichText(u"start middle"));
    bob.setTextBoxBody("box", plainRichText(u"middle end"));
    sync(alice, bob);
    sync(bob, alice);
    EXPECT_EQ(characters(alice.textBox("box")->body), u"start middle end");
    EXPECT_EQ(alice.textBox("box")->body, bob.textBox("box")->body);
}

TEST(BoardText, EquationsAndFormattingReachPeers) {
    Board alice;
    Board bob;
    alice.addTextBox(makeBox());
    RichText body = plainRichText(u"x");
    body[0].italic = true;
    body.push_back(equationUnit("y^2", false));
    alice.setTextBoxBody("box", body);
    sync(alice, bob);
    EXPECT_EQ(bob.textBox("box")->body, body);
    const Bytes saved = alice.encodeState();
    Board reloaded;
    ASSERT_TRUE(reloaded.apply(saved));
    EXPECT_EQ(reloaded.textBox("box")->body, body);
}

TEST(BoardText, UndoRevertsOneStepAtATime) {
    Board board;
    board.addTextBox(makeBox("box", u"one"));
    board.setTextBoxBody("box", plainRichText(u"one two"));
    board.endGesture();
    TextBox moved = *board.textBox("box");
    moved.x = 500.0F;
    board.setTextBoxFrame(moved);
    board.endGesture();
    ASSERT_TRUE(board.undo());
    EXPECT_FLOAT_EQ(board.textBox("box")->x, 10.0F);
    ASSERT_TRUE(board.undo());
    EXPECT_EQ(characters(board.textBox("box")->body), u"one");
    ASSERT_TRUE(board.undo());
    EXPECT_TRUE(board.textBoxes().empty());
}

TEST(BoardText, ListenerHearsChangesFromPeers) {
    Board alice;
    Board bob;
    int calls = 0;
    bob.setChangeListener([&] { ++calls; });
    alice.addTextBox(makeBox());
    sync(alice, bob);
    EXPECT_GT(calls, 0);
    EXPECT_EQ(bob.textBoxes().size(), 1U);
}

TEST(BoardText, EraserAndBoundsUseTheMeasuredHeight) {
    Board board;
    board.addTextBox(makeBox());
    EXPECT_EQ(board.eraseNear({20.0F, 200.0F}, 1.0F), 0U);
    board.setTextBoxHeight("box", 200.0F);
    EXPECT_FLOAT_EQ(board.contentBounds()->bottom, 220.0F);
    EXPECT_THROW(board.setTextBoxHeight("box", -1.0F), std::invalid_argument);
    EXPECT_EQ(board.eraseNear({20.0F, 200.0F}, 1.0F), 1U);
    EXPECT_TRUE(board.textBoxes().empty());
    EXPECT_FALSE(board.contentBounds().has_value());
}

TEST(BoardText, ChangesToMissingBoxesAreIgnored) {
    Board board;
    board.setTextBoxBody("nothing", plainRichText(u"x"));
    board.removeTextBox("nothing");
    EXPECT_TRUE(board.textBoxes().empty());
    EXPECT_FALSE(board.canUndo());
}

} // namespace
} // namespace peerchalk
