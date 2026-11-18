#include "ui/text_box_model.hpp"

#include <QColor>

namespace peerchalk {
namespace {

enum Role { Id = Qt::UserRole + 1, X, Y, Width, FontSize, Color };

} // namespace

int TextBoxModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(rows_.size());
}

QVariant TextBoxModel::data(const QModelIndex &index, int role) const {
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};
    const Row &row = rows_[std::size_t(index.row())];
    switch (role) {
    case Id: return QString::fromStdString(row.id);
    case X: return row.x;
    case Y: return row.y;
    case Width: return row.width;
    case FontSize: return row.fontSize;
    case Color: return QColor::fromRgba(row.argb);
    default: return {};
    }
}

QHash<int, QByteArray> TextBoxModel::roleNames() const {
    return {{Id, "boxId"},     {X, "boxX"},
            {Y, "boxY"},       {Width, "boxWidth"},
            {FontSize, "boxFontSize"}, {Color, "boxColor"}};
}

void TextBoxModel::sync(const std::vector<TextBox> &boxes) {
    std::size_t next = 0;
    for (const TextBox &box : boxes) {
        const Row row{box.id, box.x, box.y, box.width, box.fontSize, box.argb};
        while (next < rows_.size() && rows_[next].id < box.id) {
            beginRemoveRows({}, int(next), int(next));
            rows_.erase(rows_.begin() + std::ptrdiff_t(next));
            endRemoveRows();
        }
        if (next == rows_.size() || rows_[next].id != box.id) {
            beginInsertRows({}, int(next), int(next));
            rows_.insert(rows_.begin() + std::ptrdiff_t(next), row);
            endInsertRows();
        } else if (!(rows_[next] == row)) {
            rows_[next] = row;
            emit dataChanged(index(int(next)), index(int(next)));
        }
        ++next;
    }
    if (next < rows_.size()) {
        beginRemoveRows({}, int(next), int(rows_.size()) - 1);
        rows_.resize(next);
        endRemoveRows();
    }
}

} // namespace peerchalk
