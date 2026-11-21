#pragma once

// The Yrs side of BoardPages, shared by its source files.
#include "board/board_native.hpp"
#include "board/board_pages.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace peerchalk::page_yrs {

inline constexpr char pagesKind[] = "pages";
inline constexpr double maxPageSide = 100000.0; // about 26 m; anything larger is not paper

[[nodiscard]] std::string newId();
// Throws std::invalid_argument unless both sides are finite, positive and below maxPageSide.
void checkPaper(PaperSize paper);

// A Yrs map to insert; the keys and strings it points to live as long as it does.
class MapInput {
  public:
    void add(const char *key, YInput value) {
        keys_.push_back(const_cast<char *>(key)); // Yrs only reads the keys
        values_.push_back(value);
    }
    void addString(const char *key, const std::string &value) {
        add(key, yinput_string(strings_.emplace_back(value).c_str()));
    }
    [[nodiscard]] YInput build() {
        return yinput_ymap(keys_.data(), values_.data(), static_cast<std::uint32_t>(keys_.size()));
    }

  private:
    std::vector<char *> keys_;
    std::vector<YInput> values_;
    std::deque<std::string> strings_;
};

void writeItem(MapInput &map, const ColumnItem &item);
[[nodiscard]] const Branch *mapAt(const Branch *array, const YTransaction *txn,
                                  std::uint32_t index);
// The index of the map whose "id" is id.
[[nodiscard]] std::optional<std::uint32_t> indexOf(const Branch *array, const YTransaction *txn,
                                                   const std::string &id);
[[nodiscard]] const Branch *itemsOf(const Branch *column, const YTransaction *txn);
// What a peer wrote, or nullopt when this version cannot read it.
[[nodiscard]] std::optional<ColumnItem> readItem(const Branch *map, const YTransaction *txn);
[[nodiscard]] std::optional<ColumnSpec> readColumn(const Branch *map, const YTransaction *txn);
void createStrokeArray(const Board::Native &native, YTransaction *txn, const std::string &page);
void insertColumnSpec(const Board::Native &native, YTransaction *txn, std::uint32_t index,
                      const ColumnSpec &spec);
// A new column with one new page; returns the column's id.
std::string insertColumn(const Board::Native &native, YTransaction *txn, std::uint32_t index,
                         PaperSize paper, const std::string &title);
[[nodiscard]] const Branch *columnItems(const Board::Native &native, const YTransaction *txn,
                                        const std::string &columnId);
void removePageStrokes(const Board::Native &native, YTransaction *txn, const Branch *items);

} // namespace peerchalk::page_yrs
