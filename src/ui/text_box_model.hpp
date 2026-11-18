#pragma once

#include "board/text_box.hpp"

#include <QAbstractListModel>

#include <vector>

namespace peerchalk {

// The board's text boxes for QML. Rows stay in place while their boxes change, so the views
// showing them, and any text being edited, survive updates.
class TextBoxModel : public QAbstractListModel {
    Q_OBJECT

  public:
    using QAbstractListModel::QAbstractListModel;

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // boxes must be sorted by id.
    void sync(const std::vector<TextBox> &boxes);

  private:
    struct Row {
        std::string id;
        float x, y, width, fontSize;
        std::uint32_t argb;
        bool operator==(const Row &) const = default;
    };

    std::vector<Row> rows_;
};

} // namespace peerchalk
