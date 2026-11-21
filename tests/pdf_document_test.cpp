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
