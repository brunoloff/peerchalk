#include "board/board_pages.hpp"
#include "library/blob_store.hpp"
#include "test_pdf.hpp"
#include "ui/library.hpp"
#include "ui/pdf_renderer.hpp"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

QByteArray samplePdf() {
    const Bytes pdf = test::samplePdf();
    return {reinterpret_cast<const char *>(pdf.data()), static_cast<qsizetype>(pdf.size())};
}

TEST(PdfRenderer, RoundsHeightsUpToPowersOfTheSquareRootOfTwo) {
    EXPECT_EQ(PdfRenderer::bucketFor(10.0), 64);
    EXPECT_EQ(PdfRenderer::bucketFor(64.0), 64);
    EXPECT_EQ(PdfRenderer::bucketFor(65.0), 91);
    EXPECT_EQ(PdfRenderer::bucketFor(1000.0), 1024);
    EXPECT_EQ(PdfRenderer::bucketFor(1e6), 4096);
}

TEST(PdfRenderer, RendersWhatIsWantedAndOffersTheNearestSize) {
    QTemporaryDir dir;
    BlobStore blobs(dir.path());
    const std::string hash = blobs.put(samplePdf()).toStdString();
    PdfRenderer renderer(blobs, std::size_t(64) << 20);
    QSignalSpy rendered(&renderer, &PdfRenderer::rendered);
    EXPECT_TRUE(renderer.best({hash, 0}, 128).isNull());
    renderer.want({{{hash, 0}, 128}, {{hash, 1}, 64}});
    ASSERT_TRUE(QTest::qWaitFor([&] { return rendered.count() >= 2; }));
    const QImage first = renderer.best({hash, 0}, 128);
    EXPECT_EQ(first.height(), 128);
    EXPECT_EQ(first.width(), 91);                            // A4's proportions
    EXPECT_EQ(renderer.best({hash, 0}, 1024).height(), 128); // the nearest there is
    EXPECT_GT(renderer.best({hash, 1}, 64).width(), 64);     // the page turned a quarter
    renderer.want({{{"0000", 0}, 64}});                      // a PDF this device lacks
    ASSERT_TRUE(QTest::qWaitFor([&] { return renderer.failed("0000"); }));
    renderer.forgetFailures();
    EXPECT_FALSE(renderer.failed("0000"));
}

TEST(LibraryPdfs, AddsAPdfAsAColumnOfItsPages) {
    QTemporaryDir dir;
    Library library(dir.path());
    library.createProject("P");
    QSignalSpy errors(&library, &Library::errorOccurred);
    const QString infinite = library.createBoard("Infinite");
    EXPECT_FALSE(library.addPdf(infinite, samplePdf(), "notes", {}));
    const QString board = library.createPageBoard("Reading", "A4", false, "blank");
    EXPECT_FALSE(library.addPdf(board, "not a pdf", "notes", {}));
    EXPECT_EQ(errors.count(), 2);
    ASSERT_TRUE(library.addPdf(board, samplePdf(), "notes", {}));
    const BoardPages &pages = library.openBoard(board)->board().pages();
    ASSERT_EQ(pages.columns().size(), 2U);
    const ColumnSpec &pdf = pages.columns()[1];
    EXPECT_EQ(pdf.title, "Lemma notes");
    ASSERT_EQ(pdf.items.size(), 2U);
    EXPECT_TRUE(library.blobs().has(QString::fromStdString(pdf.items[0].pdf->blob)));
    EXPECT_FALSE(library.importPdf(board, QUrl::fromLocalFile(dir.path() + "/missing.pdf"), {}));
    QFile file(dir.path() + "/notes.pdf");
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(samplePdf());
    file.close();
    const QString started =
        library.createPageBoardFromPdf("From a PDF", QUrl::fromLocalFile(file.fileName()));
    ASSERT_FALSE(started.isEmpty());
    const BoardPages &startedPages = library.openBoard(started)->board().pages();
    ASSERT_EQ(startedPages.columns().size(), 1U);
    EXPECT_EQ(startedPages.columns()[0].title, "Lemma notes");
    EXPECT_EQ(library.boardName(started), "From a PDF");
}

} // namespace
} // namespace peerchalk
