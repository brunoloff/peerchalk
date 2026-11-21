// Library: PDFs imported into page boards, kept in the blob store.
#include "ui/library.hpp"

#include "board/board_pages.hpp"
#include "library/files.hpp"
#include "pdf/pdf_document.hpp"
#include "ui/pdf_renderer.hpp"
#include "ui/web_files.hpp"

#include <QDateTime>
#include <QFileInfo>

#include <set>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr qsizetype maxPdfBytes = qsizetype(200) << 20;

std::string refusal(PdfError error) {
    switch (error) {
    case PdfError::Password:
        return "This PDF is protected by a password, which PeerChalk cannot open";
    case PdfError::Unsupported:
        return "PDFs cannot be opened here: reload the page to try again";
    case PdfError::Unreadable:
        break;
    }
    return "This file is not a PDF, or it is damaged";
}

} // namespace

bool Library::pdfSupported() { return PdfDocument::available(); }

void Library::blobArrived() { pdfRenderer_->forgetFailures(); }

QVariantMap Library::blobProgress(const QString &hash) const {
    const auto found = blobProgress_.find(hash);
    return found != blobProgress_.end() ? found->second : QVariantMap();
}

void Library::setBlobProgress(const QString &hash, qint64 received, qint64 total,
                              const QString &from) {
    const int percent = total > 0 ? int(received * 100 / total) : 0;
    blobProgress_[hash] = {{"percent", percent}, {"from", from}};
    ++blobProgressRevision_;
    emit blobProgressChanged();
}

void Library::endBlobProgress(const QString &hash) {
    if (blobProgress_.erase(hash) == 0)
        return;
    ++blobProgressRevision_;
    emit blobProgressChanged();
}

int Library::forgetUnusedBlobs() {
    constexpr int keptDays = 30;
    int removed = 0;
    attempt([&] {
        std::set<QString> used;
        for (const BoardEntry &board : workspace_.boards()) {
            if (board.deleted())
                continue;
            for (const std::string &blob : document(board.id).board().pages().pdfBlobs())
                used.insert(QString::fromStdString(blob));
        }
        removed = blobs_.forgetUnused(used, QDateTime::currentDateTimeUtc().addDays(-keptDays));
    });
    return removed;
}

namespace {

// What a column needs from a PDF: its pages' sizes and a heading. Throws std::runtime_error
// with a message for the person when it cannot be used.
struct PdfColumn {
    std::vector<PaperSize> sizes;
    std::string title;
};

PdfColumn inspect(const QByteArray &pdf, const QString &name) {
    if (pdf.size() > maxPdfBytes)
        throw std::runtime_error("PDFs of up to 200 MB can be imported");
    const auto *begin = reinterpret_cast<const std::byte *>(pdf.constData());
    const PdfDocument::Opened opened =
        PdfDocument::open(std::make_shared<const Bytes>(begin, begin + pdf.size()));
    if (!opened.document)
        throw std::runtime_error(refusal(*opened.error));
    PdfColumn column{{}, opened.document->title()};
    for (int i = 0; i < opened.document->pageCount(); ++i)
        column.sizes.push_back(opened.document->pageSize(i));
    if (column.sizes.empty())
        throw std::runtime_error("This PDF has no pages");
    if (column.title.empty())
        column.title = name.toStdString();
    return column;
}

QByteArray readPdfFile(const QString &path) {
    if (QFileInfo(path).size() > maxPdfBytes)
        throw std::runtime_error("PDFs of up to 200 MB can be imported");
    std::optional<QByteArray> read = readFile(path);
    if (!read)
        throw std::runtime_error("The file is no longer there");
    return *std::move(read);
}

QString localPath(const QUrl &file) {
    return file.isLocalFile() ? file.toLocalFile() : file.toString();
}

} // namespace

bool Library::importPdf(const QString &boardId, const QUrl &file, const QString &before) {
    QByteArray contents;
    const QString path = localPath(file);
    if (!attempt([&] { contents = readPdfFile(path); }))
        return false;
    return addPdf(boardId, contents, QFileInfo(path).completeBaseName(), before);
}

bool Library::addPdf(const QString &boardId, const QByteArray &pdf, const QString &name,
                     const QString &before) {
    return attempt([&] {
        const PdfColumn column = inspect(pdf, name);
        BoardPages &pages = document(QUuid::fromString(boardId)).board().pages();
        if (pages.kind() != BoardKind::Pages)
            throw std::runtime_error("PDFs go on page boards");
        const QString hash = blobs_.put(pdf);
        pages.addPdfColumn(before.toStdString(), column.title, hash.toStdString(), column.sizes);
    });
}

QString Library::createPageBoardFromPdf(const QString &name, const QUrl &file) {
    QByteArray contents;
    const QString path = localPath(file);
    if (!attempt([&] { contents = readPdfFile(path); }))
        return {};
    return createPageBoardFromPdfContents(name, contents, QFileInfo(path).completeBaseName());
}

QString Library::createPageBoardFromPdfContents(const QString &name, const QByteArray &pdf,
                                                const QString &fileName) {
    std::optional<PdfColumn> column;
    if (!attempt([&] { column = inspect(pdf, fileName); }))
        return {};
    const QString id = createBoard(name);
    if (id.isEmpty() || !attempt([&] {
            const QString hash = blobs_.put(pdf);
            document(QUuid::fromString(id))
                .board()
                .pages()
                .startWithPdf(column->title, hash.toStdString(), column->sizes);
        }))
        return {};
    return id;
}

void Library::pickPdf(const QString &boardId, const QString &before) {
#ifdef __EMSCRIPTEN__
    pickFile(QStringLiteral(".pdf,application/pdf"),
             [this, boardId, before](QByteArray contents, QString fileName) {
                 if (addPdf(boardId, contents, QFileInfo(fileName).completeBaseName(), before))
                     emit pdfAdded(boardId);
             });
#else
    Q_UNUSED(boardId)
    Q_UNUSED(before)
    emit errorOccurred(tr("This file picker is only available in a browser"));
#endif
}

void Library::pickPdfForNewBoard(const QString &name) {
#ifdef __EMSCRIPTEN__
    pickFile(QStringLiteral(".pdf,application/pdf"), [this, name](QByteArray contents,
                                                                  QString fileName) {
        const QString id =
            createPageBoardFromPdfContents(name, contents, QFileInfo(fileName).completeBaseName());
        if (!id.isEmpty())
            emit pageBoardCreated(id);
    });
#else
    Q_UNUSED(name)
    emit errorOccurred(tr("This file picker is only available in a browser"));
#endif
}

} // namespace peerchalk
