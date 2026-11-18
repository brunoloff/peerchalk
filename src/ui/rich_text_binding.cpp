#include "ui/rich_text_binding.hpp"

#include "ui/board_canvas.hpp"
#include "ui/equation_syntax.hpp"
#include "ui/rich_text_document.hpp"

#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextDocument>
#include <QtLogging>

#include <algorithm>
#include <utility>

namespace peerchalk {
namespace {

constexpr int undoPauseMs = 800;
} // namespace

RichTextBinding::RichTextBinding(QObject *parent) : QObject(parent) {
    layoutTimer_.setSingleShot(true);
    layoutTimer_.setInterval(0);
    connect(&layoutTimer_, &QTimer::timeout, this, &RichTextBinding::updateLayoutInfo);
    undoTimer_.setSingleShot(true);
    undoTimer_.setInterval(undoPauseMs);
    connect(&undoTimer_, &QTimer::timeout, this, &RichTextBinding::flush);
}

void RichTextBinding::setCanvas(BoardCanvas *canvas) {
    if (canvas == canvas_)
        return;
    if (canvas_)
        disconnect(canvas_, nullptr, this, nullptr);
    canvas_ = canvas;
    if (canvas_)
        connect(canvas_, &BoardCanvas::textBoxesChanged, this, &RichTextBinding::syncFromBoard);
    attach();
}

void RichTextBinding::setBoxId(const QString &id) {
    boxId_ = id;
    attach();
}

void RichTextBinding::setDocument(QQuickTextDocument *document) {
    setTextDocument(document != nullptr ? document->textDocument() : nullptr);
}

void RichTextBinding::setTextDocument(QTextDocument *document) {
    if (document == document_)
        return;
    if (document_)
        disconnect(document_, nullptr, this, nullptr);
    document_ = document;
    attach();
}

void RichTextBinding::setFontSize(qreal size) {
    fontSize_ = size;
    relayoutEquations();
}

void RichTextBinding::attach() {
    if (!canvas_ || !document_ || boxId_.isEmpty())
        return;
    document_->setUndoRedoEnabled(false);
    QAbstractTextDocumentLayout *layout = document_->documentLayout();
    layout->registerHandler(equationObjectType, this);
    connect(document_, &QTextDocument::contentsChange, this, &RichTextBinding::documentEdited,
            Qt::UniqueConnection);
    connect(layout, &QAbstractTextDocumentLayout::documentSizeChanged, &layoutTimer_,
            qOverload<>(&QTimer::start), Qt::UniqueConnection);
    connect(layout, &QAbstractTextDocumentLayout::update, &layoutTimer_,
            qOverload<>(&QTimer::start), Qt::UniqueConnection);
    syncFromBoard();
}

void RichTextBinding::documentEdited(int position, int removed, int added) {
    layoutTimer_.start();
    if (applying_)
        return;
    // Several edits can arrive before the next push; remember the range they touched.
    const bool first = !localDirty_;
    editStart_ = first ? position : std::min(editStart_, position);
    editEnd_ = first ? position + added : std::max(editEnd_ + added - removed, position + added);
    if (!first)
        return;
    localDirty_ = true;
    QMetaObject::invokeMethod(this, &RichTextBinding::pushLocalChanges, Qt::QueuedConnection);
}

void RichTextBinding::applyPendingStyle(RichText &body) const {
    const std::size_t end = std::min(std::size_t(std::max(editEnd_, 0)), body.size());
    for (std::size_t i = std::size_t(std::max(editStart_, 0)); i < end; ++i) {
        body[i].bold = nextBold_.value_or(body[i].bold);
        body[i].italic = nextItalic_.value_or(body[i].italic);
    }
}

// Returns where the cursor belongs if the last typed character completed a block equation.
std::optional<int> RichTextBinding::convertTypedEquations(RichText &body) const {
    std::optional<int> cursor;
    auto end = std::min(std::size_t(editEnd_), body.size());
    for (std::size_t i = std::size_t(editStart_); i < end; ++i) {
        const std::size_t before = body.size();
        const std::optional<std::size_t> after = convertTypedEquation(body, i + 1);
        if (!after)
            continue;
        const bool last = i + 1 == end;
        end = std::min(end + body.size() - before, body.size());
        cursor = last && body[*after - 2].equation && body[*after - 2].displayMath
                     ? std::optional<int>(int(*after))
                     : std::nullopt;
        i = *after - 1;
    }
    return cursor;
}

void RichTextBinding::pushLocalChanges() {
    if (!localDirty_ || !document_ || !canvas_)
        return;
    localDirty_ = false;
    RichText body = readRichText(*document_);
    std::optional<int> cursor;
    if (editEnd_ > editStart_) {
        applyPendingStyle(body);
        cursor = convertTypedEquations(body);
    }
    commit(body);
    if (cursor)
        emit cursorRequested(*cursor);
    undoTimer_.start();
}

void RichTextBinding::commit(const RichText &body) {
    if (!canvas_ || !document_)
        return;
    if (body.size() > maxRichTextUnits) {
        qWarning("RichTextBinding: refusing text longer than %zu characters", maxRichTextUnits);
        syncFromBoard();
        return;
    }
    applying_ = true;
    applyRichText(*document_, body);
    applying_ = false;
    canvas_->setTextBoxBody(boxId_, body);
    layoutTimer_.start();
}

void RichTextBinding::syncFromBoard() {
    if (!canvas_ || !document_ || boxId_.isEmpty())
        return;
    pushLocalChanges();
    const TextBox *box = canvas_->board().textBox(boxId_.toStdString());
    if (box == nullptr)
        return;
    applying_ = true;
    applyRichText(*document_, box->body);
    applying_ = false;
}

void RichTextBinding::flush() {
    pushLocalChanges();
    undoTimer_.stop();
    if (canvas_)
        canvas_->endTextBoxChange();
}

bool RichTextBinding::hasStyle(Style style, int position) const {
    if (!document_)
        return false;
    const RichText body = readRichText(*document_);
    if (position < 0 || std::size_t(position) >= body.size())
        return false;
    return style == Style::Bold ? body[std::size_t(position)].bold
                                : body[std::size_t(position)].italic;
}

bool RichTextBinding::isBold(int position) const {
    return nextBold_.value_or(hasStyle(Style::Bold, position));
}

bool RichTextBinding::isItalic(int position) const {
    return nextItalic_.value_or(hasStyle(Style::Italic, position));
}

void RichTextBinding::toggle(Style style, int start, int end) {
    pushLocalChanges();
    std::optional<bool> &next = style == Style::Bold ? nextBold_ : nextItalic_;
    if (start >= end) {
        next = !next.value_or(hasStyle(style, start - 1));
        return;
    }
    RichText body = readRichText(*document_);
    const auto last = std::min(std::size_t(end), body.size());
    const auto first = std::min(std::size_t(std::max(start, 0)), last);
    const auto isOn = [&](const RichUnit &unit) {
        return unit.ch == u'\n' || (style == Style::Bold ? unit.bold : unit.italic);
    };
    const bool value = !std::all_of(body.begin() + std::ptrdiff_t(first),
                                    body.begin() + std::ptrdiff_t(last), isOn);
    for (std::size_t i = first; i < last; ++i)
        (style == Style::Bold ? body[i].bold : body[i].italic) = value;
    commit(body);
    flush();
}

void RichTextBinding::toggleBold(int start, int end) { toggle(Style::Bold, start, end); }

void RichTextBinding::toggleItalic(int start, int end) { toggle(Style::Italic, start, end); }

} // namespace peerchalk
