#include "pdf/pdf_outline.hpp"

#include "board/rich_text.hpp"

#include <algorithm>
#include <cstdio>
#include <regex>
#include <stdexcept>
#include <string_view>

namespace peerchalk {
namespace {

[[noreturn]] void unreadable() {
    throw std::invalid_argument("The PDF's structure could not be followed to add bookmarks");
}

int lastNumberAfter(std::string_view text, const std::string &key) {
    const std::size_t at = text.rfind(key);
    if (at == std::string_view::npos)
        unreadable();
    std::size_t i = at + key.size();
    while (i < text.size() && (text[i] == ' ' || text[i] == '\r' || text[i] == '\n'))
        ++i;
    std::size_t end = i;
    while (end < text.size() && text[end] >= '0' && text[end] <= '9')
        ++end;
    if (end == i)
        unreadable();
    return std::stoi(std::string(text.substr(i, end - i)));
}

// The dictionary of object `number` 0 obj, as written.
std::string objectDict(std::string_view text, int number) {
    const std::string head = std::to_string(number) + " 0 obj";
    std::size_t at = text.rfind("\n" + head);
    at = at == std::string_view::npos ? text.rfind("\r" + head) : at;
    if (at == std::string_view::npos)
        unreadable();
    const std::size_t open = text.find("<<", at);
    const std::size_t close = text.find("endobj", at);
    if (open == std::string_view::npos || close == std::string_view::npos || open > close)
        unreadable();
    const std::size_t end = text.rfind(">>", close);
    return std::string(text.substr(open, end + 2 - open));
}

int refAfter(const std::string &dict, const std::string &key) {
    std::smatch match;
    if (!std::regex_search(dict, match, std::regex(key + R"(\s*(\d+)\s+0\s+R)")))
        unreadable();
    return std::stoi(match[1]);
}

// The page objects in order, following the page tree from node.
void collectPages(std::string_view text, int node, std::vector<int> &pages, int depth) {
    const std::string dict = objectDict(text, node);
    if (depth > 32)
        unreadable();
    if (dict.find("/Type/Pages") == std::string::npos &&
        dict.find("/Type /Pages") == std::string::npos) {
        pages.push_back(node);
        return;
    }
    std::smatch kids;
    if (!std::regex_search(dict, kids, std::regex(R"(/Kids\s*\[([^\]]*)\])")))
        unreadable();
    const std::string list = kids[1];
    const std::regex ref(R"((\d+)\s+0\s+R)");
    for (std::sregex_iterator it(list.begin(), list.end(), ref), end; it != end; ++it)
        collectPages(text, std::stoi((*it)[1]), pages, depth + 1);
}

// A PDF text string in UTF-16BE, which keeps any language.
std::string pdfText(const std::string &utf8) {
    std::string hex = "<FEFF";
    char unit[5];
    for (const char16_t c : toUtf16(utf8)) {
        std::snprintf(unit, sizeof unit, "%04X", static_cast<unsigned>(c));
        hex += unit;
    }
    return hex + ">";
}

} // namespace

namespace {

struct Tree {
    std::vector<int> parent;                // -1: the outline root
    std::vector<std::vector<int>> children; // the last is the root's
};

Tree buildTree(const std::vector<PdfOutlineEntry> &entries) {
    Tree tree{std::vector<int>(entries.size(), -1),
              std::vector<std::vector<int>>(entries.size() + 1)};
    int heading = -1;
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        if (entries[std::size_t(i)].level > 1 && heading >= 0)
            tree.parent[std::size_t(i)] = heading;
        else
            heading = i;
        const int parent = tree.parent[std::size_t(i)];
        tree.children[parent < 0 ? entries.size() : std::size_t(parent)].push_back(i);
    }
    return tree;
}

std::string ref(int number) { return std::to_string(number) + " 0 R"; }

// The outline's objects: the root, then one per entry.
std::vector<std::string> outlineObjects(const std::vector<PdfOutlineEntry> &entries,
                                        const std::vector<int> &pages, int first) {
    const Tree tree = buildTree(entries);
    const auto number = [&](int entry) { return first + 1 + entry; };
    const std::vector<int> &top = tree.children.back();
    std::vector<std::string> objects{"<< /Type /Outlines /First " + ref(number(top.front())) +
                                     " /Last " + ref(number(top.back())) + " /Count " +
                                     std::to_string(entries.size()) + " >>"};
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const PdfOutlineEntry &entry = entries[std::size_t(i)];
        if (entry.page < 0 || entry.page >= static_cast<int>(pages.size()))
            throw std::invalid_argument("A bookmark leads to a page that does not exist");
        const int parent = tree.parent[std::size_t(i)];
        const std::vector<int> &siblings =
            tree.children[parent < 0 ? entries.size() : std::size_t(parent)];
        const auto at =
            std::size_t(std::find(siblings.begin(), siblings.end(), i) - siblings.begin());
        std::string dict = "<< /Title " + pdfText(entry.title) + " /Parent " +
                           ref(parent < 0 ? first : number(parent));
        if (at > 0)
            dict += " /Prev " + ref(number(siblings[at - 1]));
        if (at + 1 < siblings.size())
            dict += " /Next " + ref(number(siblings[at + 1]));
        const std::vector<int> &kids = tree.children[std::size_t(i)];
        if (!kids.empty())
            dict += " /First " + ref(number(kids.front())) + " /Last " + ref(number(kids.back())) +
                    " /Count " + std::to_string(kids.size());
        dict += " /Dest [" + ref(pages[std::size_t(entry.page)]) + " /Fit] >>";
        objects.push_back(std::move(dict));
    }
    return objects;
}

std::string xrefLine(std::size_t offset) {
    char line[21];
    std::snprintf(line, sizeof line, "%010zu 00000 n\r\n", offset);
    return line;
}

} // namespace

Bytes addPdfOutline(const Bytes &pdf, const std::vector<PdfOutlineEntry> &entries) {
    if (entries.empty())
        return pdf;
    const std::string_view text(reinterpret_cast<const char *>(pdf.data()), pdf.size());
    const int previousXref = lastNumberAfter(text, "startxref");
    const std::string_view trailer = text.substr(text.rfind("trailer"));
    const int size = lastNumberAfter(trailer, "/Size");
    const int root = lastNumberAfter(trailer, "/Root");
    std::string catalog = objectDict(text, root);
    std::vector<int> pages;
    collectPages(text, refAfter(catalog, "/Pages"), pages, 0);
    const std::vector<std::string> outline = outlineObjects(entries, pages, size);
    catalog.insert(catalog.size() - 2, " /Outlines " + ref(size) + " /PageMode /UseOutlines ");
    std::string update = "\n";
    std::vector<std::size_t> offsets;
    const auto add = [&](int number, const std::string &dict) {
        offsets.push_back(pdf.size() + update.size());
        update += std::to_string(number) + " 0 obj\n" + dict + "\nendobj\n";
    };
    add(root, catalog);
    for (std::size_t i = 0; i < outline.size(); ++i)
        add(size + static_cast<int>(i), outline[i]);
    const std::size_t xref = pdf.size() + update.size();
    update += "xref\n" + std::to_string(root) + " 1\n" + xrefLine(offsets[0]);
    update += std::to_string(size) + " " + std::to_string(outline.size()) + "\n";
    for (std::size_t i = 1; i < offsets.size(); ++i)
        update += xrefLine(offsets[i]);
    std::string info;
    if (trailer.find("/Info") != std::string_view::npos)
        info = " /Info " + ref(lastNumberAfter(trailer, "/Info"));
    update += "trailer\n<< /Size " + std::to_string(size + int(outline.size())) + " /Root " +
              ref(root) + info + " /Prev " + std::to_string(previousXref) + " >>\nstartxref\n" +
              std::to_string(xref) + "\n%%EOF\n";
    Bytes result = pdf;
    const auto *begin = reinterpret_cast<const std::byte *>(update.data());
    result.insert(result.end(), begin, begin + update.size());
    return result;
}

} // namespace peerchalk
