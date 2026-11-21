// The browser build's PDF code end to end: PdfDocument, composePdf and addPdfOutline, compiled
// for WebAssembly and reaching PDFium's own module through src/pdf/pdfium_web_bridge.cpp.
// tests/web/run_pdf_web_check.mjs builds and runs it under Node.
#include "pdf/pdf_compose.hpp"
#include "pdf/pdf_document.hpp"
#include "pdf/pdf_outline.hpp"

#include "../test_pdf.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace peerchalk;

int failures = 0;

void check(bool ok, const char *what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    failures += ok ? 0 : 1;
}

std::shared_ptr<const Bytes> shared(Bytes bytes) {
    return std::make_shared<const Bytes>(std::move(bytes));
}

} // namespace

int main() {
    check(PdfDocument::available(), "PDFium is there");
    const auto sample = shared(test::samplePdf());
    const PdfDocument::Opened opened = PdfDocument::open(sample);
    check(opened.document != nullptr, "the PDF opens");
    if (!opened.document)
        return 1;
    const PdfDocument &pdf = *opened.document;
    check(pdf.pageCount() == 2, "it has two pages");
    check(std::abs(pdf.pageSize(0).width - paperA4.width) < 0.05F, "the first is A4");
    check(pdf.pageSize(1).width == paperLetter.height, "the second is turned a quarter");
    check(pdf.title() == "Lemma notes", "its title reads");
    const PdfImage image = pdf.render(0, 200, 283);
    check(image.pixels[(std::size_t(278) * 200 + 5) * 4] == 0, "the square renders black");
    check(image.pixels[(std::size_t(20) * 200 + 150) * 4] == 0xFF, "the paper renders white");
    const PdfImage tile = pdf.renderTile(0, 400, 566, 0, 283, 200, 283);
    check(tile.pixels[(std::size_t(270) * 200 + 10) * 4] == 0, "a tile renders its part");
    const Bytes combined = composePdf(*sample, {{std::nullopt, 500.5F, 700.25F},
                                                {PdfUnderlay{sample, 0}}});
    const Bytes marked = addPdfOutline(combined, {{"Notes", 1, 0}, {"Part", 2, 1}});
    const PdfDocument::Opened result = PdfDocument::open(shared(marked));
    check(result.document != nullptr, "the combined PDF opens");
    if (result.document) {
        check(std::abs(result.document->pageSize(0).width - 500.5F * 4.0F / 3.0F) < 0.01F,
              "a page has its exact size");
        check(result.document->outline().size() == 2, "its bookmarks read back");
    }
    std::printf("%s\n", failures == 0 ? "all passed" : "some failed");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
