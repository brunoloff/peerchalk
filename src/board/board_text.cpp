// Board's text boxes. The document's "textBoxes" map holds one map per box, by id, with the
// numbers x, y, width and size, the integer color and the text "body". Bold and italic are
// text attributes; an equation is an embedded {tex, display} object of length one.
#include "board/board_native.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace peerchalk {
namespace {

constexpr char bodyKey[] = "body";
constexpr float unmeasuredLineFactor = 1.4F;

struct Chunks {
    YChunk *data;
    std::uint32_t length;
    ~Chunks() { ychunks_destroy(data, length); }
};

bool isTrue(const YOutput *value) {
    return value != nullptr && value->tag == Y_JSON_BOOL && *youtput_read_bool(value) != 0;
}

RichUnit readEquation(const YOutput &data) {
    RichUnit unit = equationUnit({}, false);
    const YMapEntry *entries = youtput_read_json_map(&data);
    for (std::uint32_t i = 0; i < data.len; ++i) {
        const std::string_view key = entries[i].key;
        if (key == "display")
            unit.displayMath = isTrue(entries[i].value);
        else if (key == "tex" && entries[i].value->tag == Y_JSON_STR)
            unit.tex = youtput_read_string(entries[i].value);
    }
    return unit;
}

void appendChunk(RichText &body, const YChunk &chunk) {
    RichUnit style;
    for (std::uint32_t i = 0; i < chunk.fmt_len; ++i) {
        const std::string_view key = chunk.fmt[i].key;
        style.bold |= key == "bold" && isTrue(chunk.fmt[i].value);
        style.italic |= key == "italic" && isTrue(chunk.fmt[i].value);
    }
    if (chunk.data.tag == Y_JSON_STR) {
        for (const char16_t ch : toUtf16(youtput_read_string(&chunk.data))) {
            style.ch = ch;
            body.push_back(style);
        }
    } else {
        // Embeds this version does not know still occupy one position.
        RichUnit unit = chunk.data.tag == Y_JSON_MAP ? readEquation(chunk.data) : style;
        unit.ch = equationChar;
        unit.bold = style.bold;
        unit.italic = style.italic;
        body.push_back(std::move(unit));
    }
}

RichText readBody(const Branch *text, const YTransaction *txn) {
    std::uint32_t length = 0;
    const Chunks chunks{ytext_chunks(text, txn, &length), length};
    RichText body;
    for (std::uint32_t i = 0; i < length; ++i)
        appendChunk(body, chunks.data[i]);
    return body;
}

// A box from a peer that does not make sense is left out.
std::optional<TextBox> readBox(const char *id, const Branch *map, const YTransaction *txn) {
    const auto x = mapNumber(map, txn, "x");
    const auto y = mapNumber(map, txn, "y");
    const auto width = mapNumber(map, txn, "width");
    const auto size = mapNumber(map, txn, "size");
    const auto color = mapNumber(map, txn, "color");
    const Output body(ymap_get(map, txn, bodyKey), youtput_destroy);
    if (!x || !y || !width || !size || !color || !body || body->tag != Y_TEXT)
        return std::nullopt;
    const Branch *text = youtput_read_ytext(body.get());
    if (ytext_len(text, txn) > maxRichTextUnits)
        return std::nullopt;
    TextBox box;
    box.id = id;
    box.x = float(*x);
    box.y = float(*y);
    box.width = std::clamp(float(*width), TextBox::minWidth, TextBox::maxWidth);
    box.fontSize = std::clamp(float(*size), TextBox::minFontSize, TextBox::maxFontSize);
    box.argb = std::uint32_t(std::int64_t(*color));
    box.body = readBody(text, txn);
    return box;
}

void requireValid(const TextBox &box) {
    const bool finite = std::isfinite(box.x) && std::isfinite(box.y) && std::isfinite(box.width) &&
                        std::isfinite(box.fontSize);
    if (box.id.empty() || !finite || box.width < TextBox::minWidth ||
        box.width > TextBox::maxWidth || box.fontSize < TextBox::minFontSize ||
        box.fontSize > TextBox::maxFontSize)
        throw std::invalid_argument("Text box id, position or size is invalid");
}

void writeFrame(const Branch *map, YTransaction *txn, const TextBox &box) {
    const YInput x = yinput_float(box.x);
    const YInput y = yinput_float(box.y);
    const YInput width = yinput_float(box.width);
    const YInput size = yinput_float(box.fontSize);
    const YInput color = yinput_long(box.argb);
    ymap_insert(map, txn, "x", &x);
    ymap_insert(map, txn, "y", &y);
    ymap_insert(map, txn, "width", &width);
    ymap_insert(map, txn, "size", &size);
    ymap_insert(map, txn, "color", &color);
}

// Attributes that set both styles, so that new text never inherits its neighbour's.
struct StyleAttributes {
    char bold[5] = "bold";
    char italic[7] = "italic";
    char *keys[2] = {bold, italic};
    YInput values[2];
    YInput input;

    explicit StyleAttributes(const RichUnit &unit)
        : values{unit.bold ? yinput_bool(Y_TRUE) : yinput_null(),
                 unit.italic ? yinput_bool(Y_TRUE) : yinput_null()},
          input(yinput_json_map(keys, values, 2)) {}
};

void insertEquation(const Branch *text, YTransaction *txn, std::uint32_t index,
                    const RichUnit &unit) {
    char texKey[] = "tex";
    char displayKey[] = "display";
    char *keys[] = {texKey, displayKey};
    YInput values[] = {yinput_string(unit.tex.c_str()),
                       yinput_bool(unit.displayMath ? Y_TRUE : Y_FALSE)};
    const YInput content = yinput_json_map(keys, values, 2);
    const StyleAttributes style(unit);
    ytext_insert_embed(text, txn, index, &content, &style.input);
}

// Calls action(first, end) for each stretch of [first, end) with one style.
template <typename Action>
void forEachRun(const RichText &body, std::size_t first, std::size_t end, Action action) {
    while (first < end) {
        std::size_t stop = first + 1;
        while (stop < end && !body[stop].equation && !body[first].equation &&
               body[stop].sameStyle(body[first]))
            ++stop;
        action(first, stop);
        first = stop;
    }
}

void insertUnits(const Branch *text, YTransaction *txn, const RichText &body, std::size_t first,
                 std::size_t end) {
    forEachRun(body, first, end, [&](std::size_t from, std::size_t to) {
        const auto index = std::uint32_t(from);
        if (body[from].equation) {
            insertEquation(text, txn, index, body[from]);
            return;
        }
        std::u16string units;
        for (std::size_t i = from; i < to; ++i)
            units += body[i].ch;
        const StyleAttributes style(body[from]);
        ytext_insert(text, txn, index, toUtf8(units).c_str(), &style.input);
    });
}

void formatUnits(const Branch *text, YTransaction *txn, const RichText &body, std::size_t first,
                 std::size_t end) {
    forEachRun(body, first, end, [&](std::size_t from, std::size_t to) {
        const StyleAttributes style(body[from]);
        ytext_format(text, txn, std::uint32_t(from), std::uint32_t(to - from), &style.input);
    });
}

} // namespace

void Board::textChanged() {
    textBoxesStale_ = true;
    if (listener_)
        listener_();
}

const std::vector<TextBox> &Board::textBoxes() const {
    if (!textBoxesStale_)
        return textBoxes_;
    textBoxes_.clear();
    const BoardTransaction txn(ydoc_read_transaction(native_->doc.get()), ytransaction_commit);
    if (!txn)
        throw std::runtime_error("Yrs board transaction could not start");
    std::unique_ptr<YMapIter, decltype(&ymap_iter_destroy)> entries(
        ymap_iter(native_->textBoxes, txn.get()), ymap_iter_destroy);
    while (YMapEntry *raw = ymap_iter_next(entries.get())) {
        const std::unique_ptr<YMapEntry, decltype(&ymap_entry_destroy)> entry(raw,
                                                                              ymap_entry_destroy);
        if (entry->value->tag != Y_MAP)
            continue;
        if (auto box = readBox(entry->key, youtput_read_ymap(entry->value), txn.get()))
            textBoxes_.push_back(*std::move(box));
    }
    std::sort(textBoxes_.begin(), textBoxes_.end(),
              [](const TextBox &a, const TextBox &b) { return a.id < b.id; });
    textBoxesStale_ = false;
    return textBoxes_;
}

const TextBox *Board::textBox(const std::string &id) const {
    const std::vector<TextBox> &boxes = textBoxes();
    const auto found =
        std::lower_bound(boxes.begin(), boxes.end(), id,
                         [](const TextBox &a, const std::string &b) { return a.id < b; });
    return found != boxes.end() && found->id == id ? &*found : nullptr;
}

Bounds Board::textBoxBounds(const TextBox &box) const {
    const auto measured = textBoxHeights_.find(box.id);
    const float height =
        measured != textBoxHeights_.end() ? measured->second : box.fontSize * unmeasuredLineFactor;
    return {box.x, box.y, box.x + box.width, box.y + height};
}

void Board::setTextBoxHeight(const std::string &id, float height) {
    if (!std::isfinite(height) || height < 0.0F)
        throw std::invalid_argument("Text box height must be finite and non-negative");
    textBoxHeights_[id] = height;
}

void Board::addTextBox(const TextBox &box) {
    requireValid(box);
    if (textBox(box.id) != nullptr)
        throw std::invalid_argument("A text box with this id already exists");
    {
        const BoardTransaction txn = native_->write(true);
        char body[] = "";
        char key[] = "body";
        char *keys[] = {key};
        YInput values[] = {yinput_ytext(body)};
        const YInput input = yinput_ymap(keys, values, 1);
        ymap_insert(native_->textBoxes, txn.get(), box.id.c_str(), &input);
        const Output created(ymap_get(native_->textBoxes, txn.get(), box.id.c_str()),
                             youtput_destroy);
        const Branch *map = youtput_read_ymap(created.get());
        writeFrame(map, txn.get(), box);
        const Output text(ymap_get(map, txn.get(), bodyKey), youtput_destroy);
        insertUnits(youtput_read_ytext(text.get()), txn.get(), box.body, 0, box.body.size());
    }
    endGesture();
}

void Board::setTextBoxFrame(const TextBox &box) {
    requireValid(box);
    if (textBox(box.id) == nullptr)
        return;
    const BoardTransaction txn = native_->write(true);
    const Output map(ymap_get(native_->textBoxes, txn.get(), box.id.c_str()), youtput_destroy);
    writeFrame(youtput_read_ymap(map.get()), txn.get(), box);
}

void Board::setTextBoxBody(const std::string &id, const RichText &body) {
    if (body.size() > maxRichTextUnits)
        throw std::invalid_argument("The text is too long");
    const TextBox *box = textBox(id);
    if (box == nullptr)
        return;
    const RichChange change = diffRichText(box->body, body);
    if (change.empty())
        return;
    const BoardTransaction txn = native_->write(true);
    const Output map(ymap_get(native_->textBoxes, txn.get(), id.c_str()), youtput_destroy);
    const Output value(ymap_get(youtput_read_ymap(map.get()), txn.get(), bodyKey), youtput_destroy);
    const Branch *text = youtput_read_ytext(value.get());
    const std::size_t end = change.start + change.added;
    if (change.sameCharacters) {
        formatUnits(text, txn.get(), body, change.start, end);
        return;
    }
    if (change.removed > 0)
        ytext_remove_range(text, txn.get(), std::uint32_t(change.start),
                           std::uint32_t(change.removed));
    insertUnits(text, txn.get(), body, change.start, end);
}

void Board::removeTextBox(const std::string &id) {
    if (textBox(id) == nullptr)
        return;
    const BoardTransaction txn = native_->write(true);
    ymap_remove(native_->textBoxes, txn.get(), id.c_str());
    textBoxHeights_.erase(id);
}

std::size_t Board::eraseTextBoxesNear(Point point, float radius) {
    std::vector<std::string> hits;
    for (const TextBox &box : textBoxes()) {
        if (textBoxBounds(box).contains(point, radius))
            hits.push_back(box.id);
    }
    for (const std::string &id : hits)
        removeTextBox(id);
    return hits.size();
}

} // namespace peerchalk
