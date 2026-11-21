#include "board/board_pages.hpp"
#include "ui/board_preview.hpp"

#include <QSignalSpy>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

QByteArray saved(const Board &board) {
    const Bytes state = board.encodeState();
    return {reinterpret_cast<const char *>(state.data()), static_cast<qsizetype>(state.size())};
}

TEST(BoardPreview, CountsAPageBoardsColumnsAndPages) {
    Board board;
    board.pages().start(paperA4, "Notes");
    const std::string column = board.pages().columns()[0].id;
    board.pages().insertItem(column, {}, ColumnItemKind::Page);
    board.pages().insertItem(column, {}, ColumnItemKind::Subheading, "Not a page");
    board.pages().addColumn({}, paperLetter, "Paper");
    BoardPreview preview;
    QSignalSpy changed(&preview, &BoardPreview::boardStateChanged);
    preview.setBoardState(saved(board));
    EXPECT_EQ(changed.size(), 1);
    EXPECT_TRUE(preview.pageBoard());
    EXPECT_EQ(preview.columnCount(), 2);
    EXPECT_EQ(preview.pageCount(), 3);
}

TEST(BoardPreview, TreatsInfiniteAndUnreadableBoardsAsInfinite) {
    Board board;
    Stroke stroke(0xFF000000U, 2.0F);
    stroke.addPoint({0.0F, 0.0F, 1.0F});
    board.addStroke(stroke);
    BoardPreview preview;
    preview.setBoardState(saved(board));
    EXPECT_FALSE(preview.pageBoard());
    EXPECT_EQ(preview.pageCount(), 0);
    preview.setBoardState("not a board");
    EXPECT_FALSE(preview.pageBoard());
    EXPECT_EQ(preview.columnCount(), 0);
}

} // namespace
} // namespace peerchalk
