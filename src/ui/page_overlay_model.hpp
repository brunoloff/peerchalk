#pragma once

#include "board/geometry.hpp"

#include <QAbstractListModel>

#include <functional>
#include <string>
#include <vector>

namespace peerchalk {

class PageMap;
struct ColumnSpec;

// What QML draws over a page board's pages, for the part in view: column headings (h1),
// subheadings (h2), the gaps where pages and subheadings can be added, and page numbers.
// Rows are kept by key, so delegates survive while the view moves.
class PageOverlayModel : public QAbstractListModel {
    Q_OBJECT

  public:
    enum class Kind : std::uint8_t { Heading, Subheading, Gap, PageNumber };

    struct Row {
        std::string key;
        Kind kind;
        std::string column;
        std::string item;    // the subheading or page; for gaps, the item after it ("" at the end)
        std::string text;    // headings and subheadings
        int number;          // page numbers, from 1 in each column
        Bounds bounds;       // board coordinates
        bool waiting{false}; // page numbers: the PDF the page shows is not on this device
        std::string blob{};  // and which PDF that is
        bool operator==(const Row &) const = default;
    };

    using QAbstractListModel::QAbstractListModel;

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // rows must be sorted by key.
    void sync(const std::vector<Row> &rows);

  private:
    enum Role {
        KindRole = Qt::UserRole + 1,
        Column,
        Item,
        Text,
        Number,
        X,
        Y,
        Width,
        Height,
        Waiting,
        Blob
    };

    std::vector<Row> rows_;
};

// The rows for the part of the board in area; columns are the board's, in layout order.
// missing tells whether a PDF, by blob, is not on this device.
[[nodiscard]] std::vector<PageOverlayModel::Row>
pageOverlayRows(const PageMap &map, const std::vector<ColumnSpec> &columns, const Bounds &area,
                const std::function<bool(const std::string &)> &missing = {});

} // namespace peerchalk
