#pragma once

#include "board/page_layout.hpp"
#include "board/stroke_codec.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace peerchalk {

// A rendered page: opaque pixels, 4 bytes each in the order B, G, R, A (Qt's RGB32 on
// little-endian machines), rows packed one after another.
struct PdfImage {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> pixels;
};

enum class PdfError : std::uint8_t {
    Unreadable,  // not a PDF, or damaged beyond repair
    Password,    // encrypted with a password
    Unsupported, // this build has no PDF engine
};

// A PDF read with PDFium. PDFium is not thread-safe, so every call takes one lock shared by
// all documents: they may be used from any thread, one call at a time.
class PdfDocument {
  public:
    struct Opened {
        std::unique_ptr<PdfDocument> document;
        std::optional<PdfError> error; // set when document is null
    };

    // Opens the PDF in data, which the document keeps.
    [[nodiscard]] static Opened open(std::shared_ptr<const Bytes> data);
    // Whether this build can open PDFs at all.
    [[nodiscard]] static bool available();

    ~PdfDocument();
    PdfDocument(const PdfDocument &) = delete;
    PdfDocument &operator=(const PdfDocument &) = delete;

    [[nodiscard]] int pageCount() const;
    // The page's size as shown (after its rotation), in board units. Throws
    // std::out_of_range for a page that does not exist.
    [[nodiscard]] PaperSize pageSize(int index) const;
    // The title in the PDF's information, or "".
    [[nodiscard]] std::string title() const;
    // The page at width × height pixels on white. Throws std::out_of_range for a page that
    // does not exist and std::invalid_argument for sizes outside 1 to 16384 pixels.
    [[nodiscard]] PdfImage render(int index, int width, int height) const;

  private:
    struct Native;
    explicit PdfDocument(std::unique_ptr<Native> native);

    std::unique_ptr<Native> native_;
};

} // namespace peerchalk
