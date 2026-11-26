#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

class QQmlEngine;
class QJSEngine;

namespace peerchalk {

// What the pages know of an app that embeds PeerChalk (docs/embedding.md): whether there is
// one, and where the board's Back button leads. Back then goes to the host, which leaves the
// board, rather than to PeerChalk's own home screen. The app controller sets the label and
// listens for backRequested.
class HostLink : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool embedded READ embedded CONSTANT)
    // What Back leads to, such as "Boards"; empty: the host offers no way back.
    Q_PROPERTY(QString backLabel READ backLabel NOTIFY backLabelChanged)

  public:
    explicit HostLink(bool embedded, QObject *parent = nullptr);
    // Embedded when the engine's "hosted" property is true.
    static HostLink *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    [[nodiscard]] bool embedded() const { return embedded_; }
    [[nodiscard]] QString backLabel() const { return backLabel_; }
    void setBackLabel(const QString &label);
    // The user pressed Back on the board; only meaningful when embedded.
    Q_INVOKABLE void back();

  signals:
    void backLabelChanged();
    void backRequested();

  private:
    bool embedded_;
    QString backLabel_;
};

} // namespace peerchalk
