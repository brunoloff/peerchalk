#pragma once

#include "board/board.hpp"
#include "board/page_layout.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace peerchalk {

enum class BoardKind : std::uint8_t { Infinite, Pages };

// The page-board side of a Board's document: columns of pages and subheadings, and the
// strokes on each page in page coordinates (see docs/page-boards.md).
//
// "columns" is an array of maps {id, title, paperWidth, paperHeight, items}, items an array
// of maps {id, kind: "page" | "subheading", text, width?, height?}. "pageStrokes" maps a page
// id to its array of encodeStroke() binaries, created with the page so that peers never
// race to create it. Everything is found by id, so a peer's entries this version cannot
// read are skipped without shifting anything.
//
// Edits are local changes: each structural edit is its own undo step, and strokes follow the
// Board's gestures. Unknown ids are ignored; invalid input throws std::invalid_argument.
// Reads must not happen from the Board's change listener.
class BoardPages {
  public:
    BoardPages(Board::Native &native, std::function<void()> changed);
    ~BoardPages();
    BoardPages(const BoardPages &) = delete;
    BoardPages &operator=(const BoardPages &) = delete;

    [[nodiscard]] BoardKind kind() const;
    // Makes an empty board a page board with one column of one page. Not an undo step: a
    // board is created as one kind. Throws std::logic_error unless the board is empty.
    void start(PaperSize paper, const std::string &title);

    [[nodiscard]] const std::vector<ColumnSpec> &columns() const;
    // Adds a column with one page before the column `before` (or last), returning its id.
    std::string addColumn(const std::string &before, PaperSize paper, const std::string &title);
    void removeColumn(const std::string &columnId);
    void setColumnTitle(const std::string &columnId, const std::string &title);
    // Changes the column's width and the size of its pages that have no size of their own.
    void setColumnPaper(const std::string &columnId, PaperSize paper);
    // Adds a page or subheading before the item `before` (or last), returning its id.
    std::string insertItem(const std::string &columnId, const std::string &before,
                           ColumnItemKind kind, const std::string &text = {});
    void removeItem(const std::string &columnId, const std::string &itemId);
    void setSubheadingText(const std::string &columnId, const std::string &itemId,
                           const std::string &text);
    // Moves an item before the item `before` of column `toColumn` (or last); a page keeps its
    // strokes. Yrs has no move, so the item is removed and inserted again: a peer's change
    // to that same item at the same moment is lost.
    void moveItem(const std::string &fromColumn, const std::string &itemId,
                  const std::string &toColumn, const std::string &before);
    // Moves a column before the column `before` (or last), the same way.
    void moveColumn(const std::string &columnId, const std::string &before);

    // A page's strokes, decoded the first time they are asked for after a change.
    [[nodiscard]] const std::vector<Stroke> &strokes(const std::string &pageId) const;
    void addStroke(const std::string &pageId, const Stroke &stroke);
    // Removes the page's strokes near point (page coordinates) and returns how many; they
    // join the Board's current undo step.
    std::size_t eraseNear(const std::string &pageId, Point point, float radius);

  private:
    void columnsChanged();
    void strokesChanged(std::uint32_t count, const void *events);

    Board::Native &native_;
    std::function<void()> changed_;
    mutable std::vector<ColumnSpec> columns_;
    mutable bool columnsStale_{true};
    mutable std::map<std::string, std::vector<Stroke>> strokes_;
    mutable std::set<std::string> staleStrokes_;
};

} // namespace peerchalk
