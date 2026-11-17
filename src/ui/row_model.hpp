#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace peerchalk {

// A read-only list model for QML whose rows are maps from role name to value. The
// owner replaces all rows at once with setRows.
class RowModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by Library")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

  public:
    RowModel(QList<QByteArray> roles, QObject *parent);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setRows(QList<QVariantMap> rows);

  signals:
    void countChanged();

  private:
    QList<QByteArray> roles_;
    QList<QVariantMap> rows_;
};

} // namespace peerchalk
