#pragma once

#include "math/math_engine.hpp"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <QtQml/qqmlregistration.h>
#include <functional>

namespace peerchalk {

class BoardCanvas;

// Exports the canvas's board as a PDF (see board_pdf.hpp). Equations are laid out first, so an
// export finishes later, with `finished`.
class BoardExporter : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(peerchalk::BoardCanvas *canvas MEMBER canvas_)
    Q_PROPERTY(QString fontFamily MEMBER fontFamily_)
    // Page boards: export only the column with this id; "" for the whole board.
    Q_PROPERTY(QString columnId MEMBER columnId_)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

  public:
    explicit BoardExporter(QObject *parent = nullptr);

    [[nodiscard]] bool busy() const { return busy_; }

    // Writes the PDF to a local file (desktop and mobile).
    Q_INVOKABLE void exportToFile(const QUrl &file, const QString &title);
    // Offers the PDF as a download named after the title (browsers).
    Q_INVOKABLE void download(const QString &title);
    Q_INVOKABLE [[nodiscard]] static QString fileName(const QString &title);

  signals:
    void busyChanged();
    // ok is false when nothing was written; message says why.
    void finished(bool ok, const QString &message);

  private:
    void start(std::function<void(const QByteArray &)> deliver, const QString &title);
    void formulaReady(const QString &key, MathFormula formula);
    void finish();

    QPointer<BoardCanvas> canvas_;
    QString fontFamily_;
    QString columnId_;
    QString title_;
    bool busy_{false};
    int waiting_{0};
    QHash<QString, MathFormula> formulas_;
    std::function<void(const QByteArray &)> deliver_;
};

} // namespace peerchalk
