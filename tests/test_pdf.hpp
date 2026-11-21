#pragma once

// A small PDF for tests: an A4 page with a black square in its bottom-left corner, then a
// Letter page turned a quarter, titled "Lemma notes". padding adds an unused stream of that
// many bytes, for a larger file.
#include "board/stroke_codec.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace peerchalk::test {

inline Bytes samplePdf(std::size_t padding = 0) {
    const std::string stream = "0 0 0 rg 0 0 100 100 re f";
    const std::vector<std::string> objects = {
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595.28 841.89] /Contents 5 0 R >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Rotate 90 >>",
        "<< /Length " + std::to_string(stream.size()) + " >>\nstream\n" + stream + "\nendstream",
        "<< /Title (Lemma notes) >>",
        "<< /Length " + std::to_string(padding) + " >>\nstream\n" + std::string(padding, 'x') +
            "\nendstream"};
    std::string pdf = "%PDF-1.4\n";
    std::vector<std::size_t> offsets;
    for (std::size_t i = 0; i < objects.size(); ++i) {
        offsets.push_back(pdf.size());
        pdf += std::to_string(i + 1) + " 0 obj\n" + objects[i] + "\nendobj\n";
    }
    const std::size_t xref = pdf.size();
    pdf += "xref\n0 " + std::to_string(objects.size() + 1) + "\n0000000000 65535 f \n";
    for (const std::size_t offset : offsets) {
        char line[21];
        std::snprintf(line, sizeof line, "%010zu 00000 n \n", offset);
        pdf += line;
    }
    pdf += "trailer\n<< /Size " + std::to_string(objects.size() + 1) +
           " /Root 1 0 R /Info 6 0 R >>\nstartxref\n" + std::to_string(xref) + "\n%%EOF\n";
    const auto *begin = reinterpret_cast<const std::byte *>(pdf.data());
    return {begin, begin + pdf.size()};
}

} // namespace peerchalk::test
