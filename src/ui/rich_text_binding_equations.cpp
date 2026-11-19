// The equations of RichTextBinding: editing them, measuring them and finding their places.
#include "ui/board_canvas.hpp"
#include "ui/equation_geometry.hpp"
#include "ui/equation_syntax.hpp"
#include "ui/rich_text_binding.hpp"
#include "ui/rich_text_document.hpp"

#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QtLogging>

#include <algorithm>
#include <utility>

namespace peerchalk {
namespace {

constexpr double failedCharacterWidthEm = 0.55;

QString metricsKey(const QString &tex, bool display) {
    return (display ? QLatin1Char('D') : QLatin1Char('I')) + tex;
}

} // namespace

// An equation's size lives in its block's layout, and TextEdit redraws only text reported as
// changed, so each equation is marked changed for the layout and the drawing to catch up.
void RichTextBinding::relayoutEquations() {
    if (document_) {
        const RichText body = readRichText(*document_);
        const bool wasApplying = std::exchange(applying_, true);
        QTextCursor cursor(document_);
        cursor.beginEditBlock();
        ++equationRevision_;
        QTextCharFormat format;
        format.setProperty(equationRevisionProperty, equationRevision_);
        for (std::size_t i = 0; i < body.size(); ++i) {
            if (!body[i].equation)
                continue;
            cursor.setPosition(int(i));
            cursor.setPosition(int(i) + 1, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(format);
        }
        cursor.endEditBlock();
        applying_ = wasApplying;
    }
    layoutTimer_.start();
}

void RichTextBinding::insertEquation(int position, const QString &tex, bool display) {
    pushLocalChanges();
    RichText body = readRichText(*document_);
    const auto index = std::min(std::size_t(std::max(position, 0)), body.size());
    const std::size_t placed = placeEquation(body, index, equationUnit(tex.toStdString(), display));
    commit(body);
    flush();
    emit cursorRequested(int(placed) + (display ? 2 : 1));
}

int RichTextBinding::replaceEquation(int position, const QString &tex, bool display) {
    pushLocalChanges();
    RichText body = readRichText(*document_);
    if (position < 0 || std::size_t(position) >= body.size() ||
        !body[std::size_t(position)].equation) {
        qWarning("RichTextBinding: no equation at position %d", position);
        return -1;
    }
    RichUnit unit = body[std::size_t(position)];
    unit.tex = tex.toStdString();
    unit.displayMath = display;
    body.erase(body.begin() + position);
    const std::size_t placed = placeEquation(body, std::size_t(position), std::move(unit));
    commit(body);
    flush();
    return int(placed);
}

int RichTextBinding::focusedEquation() const {
    return equationFocus_ ? equationFocus_->position() : -1;
}

int RichTextBinding::equationReturnPosition() const {
    const int position = focusedEquation();
    return position < 0 ? -1 : position + (returnAfter_ ? 1 : 0);
}

QString RichTextBinding::focusedEquationTex() const {
    const RichText body = document_ ? readRichText(*document_) : RichText{};
    const int position = focusedEquation();
    if (position < 0 || std::size_t(position) >= body.size())
        return {};
    return QString::fromStdString(body[std::size_t(position)].tex);
}

bool RichTextBinding::focusedEquationDisplay() const {
    const RichText body = document_ ? readRichText(*document_) : RichText{};
    const int position = focusedEquation();
    return position >= 0 && std::size_t(position) < body.size() &&
           body[std::size_t(position)].displayMath;
}

void RichTextBinding::focusEquation(int position, bool returnAfter) {
    if (!document_)
        return;
    const RichText body = readRichText(*document_);
    if (position < 0 || std::size_t(position) >= body.size() ||
        !body[std::size_t(position)].equation) {
        qWarning("RichTextBinding: no equation at position %d", position);
        return;
    }
    equationFocus_ = QTextCursor(document_);
    equationFocus_->setPosition(position);
    returnAfter_ = returnAfter;
    reportedFocus_ = position;
    emit focusedEquationChanged();
}

void RichTextBinding::clearEquationFocus() {
    if (!equationFocus_)
        return;
    equationFocus_.reset();
    reportedFocus_ = -1;
    emit focusedEquationChanged();
}

// Other people's edits can move the focused equation or remove it.
void RichTextBinding::checkEquationFocus() {
    if (!equationFocus_ || !document_)
        return;
    const RichText body = readRichText(*document_);
    const int position = focusedEquation();
    if (std::size_t(position) >= body.size() || !body[std::size_t(position)].equation) {
        clearEquationFocus();
    } else if (position != reportedFocus_) {
        reportedFocus_ = position;
        emit focusedEquationChanged();
    }
}

int RichTextBinding::moveAcrossEquation(int cursor, int direction) {
    if (!document_ || (direction != -1 && direction != 1))
        return -1;
    pushLocalChanges();
    const int focused = focusedEquation();
    if (focused >= 0) {
        clearEquationFocus();
        return direction > 0 ? focused + 1 : focused;
    }
    const RichText body = readRichText(*document_);
    const int target = direction > 0 ? cursor : cursor - 1;
    if (target < 0 || std::size_t(target) >= body.size() || !body[std::size_t(target)].equation)
        return -1;
    focusEquation(target, direction < 0);
    return cursor;
}

void RichTextBinding::removeFocusedEquation() {
    const int position = focusedEquation();
    if (position < 0)
        return;
    pushLocalChanges();
    clearEquationFocus();
    RichText body = readRichText(*document_);
    if (std::size_t(position) < body.size() && body[std::size_t(position)].equation)
        body.erase(body.begin() + position);
    commit(body);
    flush();
}

const RichTextBinding::Metrics &RichTextBinding::metricsFor(const QString &tex, bool display) {
    const QString key = metricsKey(tex, display);
    const auto found = metrics_.constFind(key);
    if (found != metrics_.cend())
        return *found;
    layoutMath({tex, display}, this,
               [this, key](const MathFormula &formula) { metricsArrived(key, formula); });
    return *metrics_.insert(key, Metrics{});
}

void RichTextBinding::metricsArrived(const QString &key, const MathFormula &formula) {
    Metrics metrics;
    metrics.ready = true;
    metrics.failed = !formula.error.isEmpty();
    metrics.width = metrics.failed ? double(key.size()) * failedCharacterWidthEm : formula.width;
    if (!metrics.failed) {
        metrics.height = formula.height;
        metrics.depth = formula.depth;
    }
    metrics_.insert(key, metrics);
    relayoutEquations();
}

// Qt centres an object xHeight / 2 above the baseline, so a box symmetric about that line
// keeps the formula's own baseline on the text's.
QSizeF RichTextBinding::intrinsicSize(QTextDocument *document, int, const QTextFormat &format) {
    const Metrics &m = metricsFor(format.stringProperty(equationTexProperty),
                                  format.boolProperty(equationDisplayProperty));
    return equationObjectSize(m.width, m.height, m.depth, fontSize_,
                              QFontMetricsF(document->defaultFont()).xHeight());
}

void RichTextBinding::updateLayoutInfo() {
    if (!document_ || !canvas_)
        return;
    checkEquationFocus();
    QAbstractTextDocumentLayout *layout = document_->documentLayout();
    canvas_->setTextBoxHeight(boxId_, layout->documentSize().height());
    QVariantList entries;
    const RichText body = readRichText(*document_);
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (!body[i].equation)
            continue;
        const QString tex = QString::fromStdString(body[i].tex);
        const Metrics &m = metricsFor(tex, body[i].displayMath);
        const QTextBlock block = document_->findBlock(int(i));
        const QTextLine line = block.layout()->lineForTextPosition(int(i) - block.position());
        if (!line.isValid())
            continue;
        const QPointF origin = layout->blockBoundingRect(block).topLeft();
        const qreal x = origin.x() + line.cursorToX(int(i) - block.position());
        const qreal baseline = origin.y() + line.y() + line.ascent();
        entries.push_back(QVariantMap{{QStringLiteral("position"), int(i)},
                                      {QStringLiteral("x"), x + equationPaddingEm * fontSize_},
                                      {QStringLiteral("y"), baseline - m.height * fontSize_},
                                      {QStringLiteral("tex"), tex},
                                      {QStringLiteral("display"), body[i].displayMath},
                                      {QStringLiteral("failed"), m.failed}});
    }
    if (entries != equations_) {
        equations_ = std::move(entries);
        emit equationsChanged();
    }
}

} // namespace peerchalk
