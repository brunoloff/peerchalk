// Library: PDFs imported into page boards, kept in the blob store.
#include "ui/library.hpp"

#include "board/board_pages.hpp"
#include "library/files.hpp"
#include "pdf/pdf_document.hpp"
#include "ui/pdf_renderer.hpp"

#include <QFileInfo>

#include <stdexcept>

namespace peerchalk {
namespace {

constexpr qsizetype maxPdfBytes = qsizetype(200) << 20;

std::string refusal(PdfError error) {
    switch (error) {
    case PdfError::Password:
        return "This PDF is protected by a password, which PeerChalk cannot open";
    case PdfError::Unsupported:
        return "This version of PeerChalk cannot show PDFs";
    case PdfError::Unreadable:
        break;
    }
    return "This file is not a PDF, or it is damaged";
}

} // namespace

bool Library::pdfSupported() { return PdfDocument::available(); }

void Library::blobArrived() { pdfRenderer_->forgetFailures(); }

bool Library::importPdf(const QString &boardId, const QUrl &file, const QString &before) {
    QByteArray contents;
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (!attempt([&] {
            if (QFileInfo(path).size() > maxPdfBytes)
                throw std::runtime_error("PDFs of up to 200 MB can be imported");
            std::optional<QByteArray> read = readFile(path);
            if (!read)
                throw std::runtime_error("The file is no longer there");
            contents = *std::move(read);
        }))
        return false;
    return addPdf(boardId, contents, QFileInfo(path).completeBaseName(), before);
}

bool Library::addPdf(const QString &boardId, const QByteArray &pdf, const QString &name,
                     const QString &before) {
    return attempt([&] {
        if (pdf.size() > maxPdfBytes)
            throw std::runtime_error("PDFs of up to 200 MB can be imported");
        const auto *begin = reinterpret_cast<const std::byte *>(pdf.constData());
        const PdfDocument::Opened opened =
            PdfDocument::open(std::make_shared<const Bytes>(begin, begin + pdf.size()));
        if (!opened.document)
            throw std::runtime_error(refusal(*opened.error));
        std::vector<PaperSize> sizes;
        for (int i = 0; i < opened.document->pageCount(); ++i)
            sizes.push_back(opened.document->pageSize(i));
        if (sizes.empty())
            throw std::runtime_error("This PDF has no pages");
        std::string title = opened.document->title();
        if (title.empty())
            title = name.toStdString();
        BoardPages &pages = document(QUuid::fromString(boardId)).board().pages();
        if (pages.kind() != BoardKind::Pages)
            throw std::runtime_error("PDFs go on page boards");
        const QString hash = blobs_.put(pdf);
        pages.addPdfColumn(before.toStdString(), title, hash.toStdString(), sizes);
    });
}

} // namespace peerchalk
