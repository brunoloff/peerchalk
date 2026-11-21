#include "ui/board_exporter.hpp"

#include "library/blob_store.hpp"
#include "library/files.hpp"
#include "ui/board_canvas.hpp"
#include "ui/board_pdf.hpp"
#include "ui/pdf_renderer.hpp"
#include "ui/web_files.hpp"

#include <QRegularExpression>

#include <exception>
#include <utility>

namespace peerchalk {
namespace {

// Splits a key from pdfFormulaKey back into its colour, style and source.
MathRequest requestFor(const QString &key) {
    const QStringList parts = key.split(QLatin1Char('|'));
    MathRequest request;
    request.color = parts.value(0).toUInt(nullptr, 16);
    request.display = parts.value(1) == QLatin1String("1");
    request.tex = key.section(QLatin1Char('|'), 2);
    request.tolerance = 0.0005F; // printing keeps curves smooth when zoomed in
    return request;
}

} // namespace

BoardExporter::BoardExporter(QObject *parent) : QObject(parent) {}

QString BoardExporter::fileName(const QString &title) {
    QString name = title.trimmed();
    name.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|\x00-\x1f]+)")),
                 QStringLiteral(" "));
    return (name.isEmpty() ? QStringLiteral("Board") : name.simplified()) + QStringLiteral(".pdf");
}

void BoardExporter::exportToFile(const QUrl &file, const QString &title) {
    const QString path = file.toLocalFile();
    start([path](const QByteArray &pdf) { writeFile(path, pdf, false); }, title);
}

void BoardExporter::download(const QString &title) {
#ifdef __EMSCRIPTEN__
    start([title](const QByteArray &pdf) { downloadFile(fileName(title), pdf); }, title);
#else
    Q_UNUSED(title)
    emit finished(false, tr("Downloads are only available in a browser"));
#endif
}

void BoardExporter::start(std::function<void(const QByteArray &)> deliver, const QString &title) {
    if (busy_ || !canvas_) {
        emit finished(false, busy_ ? tr("An export is already running") : tr("No board to export"));
        return;
    }
    busy_ = true;
    emit busyChanged();
    deliver_ = std::move(deliver);
    title_ = title;
    formulas_.clear();
    const QStringList keys = pdfFormulaKeys(canvas_->board());
    waiting_ = int(keys.size());
    for (const QString &key : keys) {
        layoutMath(requestFor(key), this,
                   [this, key](MathFormula formula) { formulaReady(key, std::move(formula)); });
    }
    if (waiting_ == 0)
        finish();
}

void BoardExporter::formulaReady(const QString &key, MathFormula formula) {
    formulas_.insert(key, std::move(formula));
    if (--waiting_ == 0)
        finish();
}

void BoardExporter::finish() {
    bool ok = false;
    QString message;
    try {
        if (!canvas_)
            throw std::invalid_argument("The board was closed");
        BoardPdfInput input{&canvas_->board(), std::move(formulas_), fontFamily_, title_};
        if (const PdfRenderer *pdfs = canvas_->document()->pdfRenderer())
            input.pdfBlob = [&blobs = pdfs->blobs()](const std::string &hash) {
                return blobs.get(QString::fromStdString(hash));
            };
        deliver_(renderBoardPdf(input));
        ok = true;
        message = tr("Exported %1").arg(fileName(title_));
    } catch (const std::exception &error) {
        message = tr("Could not export the board: %1").arg(QString::fromUtf8(error.what()));
    }
    formulas_.clear();
    deliver_ = {};
    busy_ = false;
    emit busyChanged();
    emit finished(ok, message);
}

} // namespace peerchalk
