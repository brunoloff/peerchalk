#include "pdf/pdf_compose.hpp"
#include "pdf/pdf_document.hpp"

#include "test_pdf.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <stdexcept>

namespace peerchalk {
namespace {

std::shared_ptr<const Bytes> shared(Bytes bytes) {
    return std::make_shared<const Bytes>(std::move(bytes));
}

TEST(PdfDocument, ReadsPagesSizesAndTitle) {
    ASSERT_TRUE(PdfDocument::available());
    const PdfDocument::Opened opened = PdfDocument::open(shared(test::samplePdf()));
    ASSERT_TRUE(opened.document) << "error " << int(opened.error.value_or(PdfError::Unsupported));
    const PdfDocument &pdf = *opened.document;
    ASSERT_EQ(pdf.pageCount(), 2);
    EXPECT_NEAR(pdf.pageSize(0).width, paperA4.width, 0.05F); // 595.28 pt is 210 mm
    EXPECT_NEAR(pdf.pageSize(0).height, paperA4.height, 0.05F);
    EXPECT_FLOAT_EQ(pdf.pageSize(1).width, paperLetter.height); // turned a quarter
    EXPECT_EQ(pdf.title(), "Lemma notes");
    EXPECT_THROW((void)pdf.pageSize(2), std::out_of_range);
}

TEST(PdfDocument, RendersPagesOnWhite) {
    const PdfDocument::Opened opened = PdfDocument::open(shared(test::samplePdf()));
    ASSERT_TRUE(opened.document);
    const PdfImage image = opened.document->render(0, 200, 283);
    ASSERT_EQ(image.width, 200);
    ASSERT_EQ(image.pixels.size(), 200U * 283U * 4U);
    const auto pixel = [&](int x, int y) { return image.pixels[(std::size_t(y) * 200 + x) * 4]; };
    EXPECT_EQ(pixel(5, 278), 0);     // in the black square at the bottom left
    EXPECT_EQ(pixel(150, 20), 0xFF); // white paper
    EXPECT_THROW((void)opened.document->render(0, 0, 10), std::invalid_argument);
    EXPECT_THROW((void)opened.document->render(5, 10, 10), std::out_of_range);
}

TEST(PdfCompose, PutsPagesOfOtherPdfsUnderTheOverlay) {
    const auto sample = shared(test::samplePdf());
    // The overlay's second page goes over the sample's first page.
    const Bytes combined = composePdf(*sample, {{}, {PdfUnderlay{sample, 0}}});
    const PdfDocument::Opened opened = PdfDocument::open(shared(combined));
    ASSERT_TRUE(opened.document);
    ASSERT_EQ(opened.document->pageCount(), 2);
    EXPECT_NEAR(opened.document->pageSize(1).width, paperA4.width, 0.05F); // the page below
    const PdfImage under = opened.document->render(1, 200, 283);
    EXPECT_EQ(under.pixels[(std::size_t(278) * 200 + 5) * 4], 0); // its black square stays
    EXPECT_THROW((void)composePdf(*sample, {{}}), std::invalid_argument);
    EXPECT_THROW((void)composePdf(*sample, {{}, {PdfUnderlay{sample, 9}}}), std::invalid_argument);
    const Bytes sized = composePdf(*sample, {{std::nullopt, 500.5F, 700.25F}, {}});
    const PdfDocument::Opened exact = PdfDocument::open(shared(sized));
    ASSERT_TRUE(exact.document);
    EXPECT_FLOAT_EQ(exact.document->pageSize(0).width, 500.5F * 4.0F / 3.0F);
}

TEST(PdfOutline, AddsHeadingsAndSubheadingsAsBookmarks) {
    const std::vector<PdfOutlineEntry> entries = {
        {"Lecture notes", 1, 0}, {"1 · The model", 2, 0}, {"Ωmega", 2, 1}, {"Paper", 1, 1}};
    const Bytes marked = addPdfOutline(test::samplePdf(), entries);
    const PdfDocument::Opened opened = PdfDocument::open(shared(marked));
    ASSERT_TRUE(opened.document);
    EXPECT_EQ(opened.document->pageCount(), 2);
    EXPECT_EQ(opened.document->outline(), entries);
    EXPECT_EQ(addPdfOutline(test::samplePdf(), {}), test::samplePdf());
    EXPECT_THROW((void)addPdfOutline(test::samplePdf(), {{"Nowhere", 1, 5}}),
                 std::invalid_argument);
    const Bytes notPdf{std::byte{'x'}};
    EXPECT_THROW((void)addPdfOutline(notPdf, entries), std::invalid_argument);
}

TEST(PdfDocument, RefusesWhatIsNotAPdf) {
    const auto bytes = [](const char *text) {
        const auto *begin = reinterpret_cast<const std::byte *>(text);
        return shared(Bytes(begin, begin + std::strlen(text)));
    };
    EXPECT_EQ(PdfDocument::open(bytes("hello")).error, PdfError::Unreadable);
    EXPECT_EQ(PdfDocument::open(shared({})).error, PdfError::Unreadable);
    EXPECT_EQ(PdfDocument::open(nullptr).error, PdfError::Unreadable);
}

} // namespace
} // namespace peerchalk
