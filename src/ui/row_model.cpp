#include "ui/row_model.hpp"

namespace peerchalk {

RowModel::RowModel(QList<QByteArray> roles, QObject *parent)
    : QAbstractListModel(parent), roles_(std::move(roles)) {}

int RowModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant RowModel::data(const QModelIndex &index, int role) const {
    const int column = role - Qt::UserRole;
    if (!index.isValid() || index.row() >= rows_.size() || column < 0 || column >= roles_.size())
        return {};
    return rows_.at(index.row()).value(QString::fromLatin1(roles_.at(column)));
}

QHash<int, QByteArray> RowModel::roleNames() const {
    QHash<int, QByteArray> names;
    for (qsizetype i = 0; i < roles_.size(); ++i)
        names.insert(Qt::UserRole + static_cast<int>(i), roles_.at(i));
    return names;
}

void RowModel::setRows(QList<QVariantMap> rows) {
    const bool countChanges = rows.size() != rows_.size();
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
    if (countChanges)
        emit countChanged();
}

} // namespace peerchalk
