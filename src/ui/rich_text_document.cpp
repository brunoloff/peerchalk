#include "ui/rich_text_document.hpp"

#include <QFont>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>

namespace peerchalk {
namespace {

QTextCharFormat styleFormat(const RichUnit &unit) {
    QTextCharFormat format;
    format.setFontWeight(unit.bold ? QFont::Bold : QFont::Normal);
    format.setFontItalic(unit.italic);
    return format;
}

QTextCharFormat equationFormat(const RichUnit &unit) {
    QTextCharFormat format = styleFormat(unit);
    format.setObjectType(equationObjectType);
    format.setProperty(equationTexProperty, QString::fromStdString(unit.tex));
    format.setProperty(equationDisplayProperty, unit.displayMath);
    format.setVerticalAlignment(QTextCharFormat::AlignMiddle);
    return format;
}

void appendFragment(RichText &out, const QTextFragment &fragment) {
    const QTextCharFormat format = fragment.charFormat();
    const bool equation = format.objectType() == equationObjectType;
    RichUnit unit;
    if (equation) {
        unit = equationUnit(format.stringProperty(equationTexProperty).toStdString(),
                            format.boolProperty(equationDisplayProperty));
    }
    unit.bold = format.fontWeight() >= QFont::Bold;
    unit.italic = format.fontItalic();
    const QString text = fragment.text();
    for (const QChar ch : text) {
        unit.ch = ch.unicode();
        out.push_back(unit);
    }
}

void insertUnits(QTextCursor &cursor, const RichText &text, std::size_t first, std::size_t end) {
    QString pending;
    QTextCharFormat format;
    const auto flush = [&] {
        if (!pending.isEmpty())
            cursor.insertText(pending, format);
        pending.clear();
    };
    for (std::size_t i = first; i < end; ++i) {
        const RichUnit &unit = text[i];
        if (unit.equation) {
            flush();
            cursor.insertText(QString(QChar::ObjectReplacementCharacter), equationFormat(unit));
        } else if (unit.ch == u'\n') {
            flush();
            cursor.insertBlock();
        } else {
            if (i == first || !unit.sameStyle(text[i - 1]) || text[i - 1].equation) {
                flush();
                format = styleFormat(unit);
            }
            pending += QChar(unit.ch);
        }
    }
    flush();
}

bool holdsOnlyDisplayEquation(const QTextBlock &block) {
    const auto it = block.begin();
    if (block.length() != 2 || it.atEnd())
        return false;
    const QTextCharFormat format = it.fragment().charFormat();
    return format.objectType() == equationObjectType &&
           format.boolProperty(equationDisplayProperty);
}

void alignEquationBlocks(QTextDocument &document) {
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        const Qt::Alignment wanted = holdsOnlyDisplayEquation(block) ? Qt::AlignHCenter
                                                                     : Qt::AlignLeft;
        if (block.blockFormat().alignment() == wanted)
            continue;
        QTextBlockFormat format;
        format.setAlignment(wanted);
        QTextCursor(block).mergeBlockFormat(format);
    }
}

} // namespace

RichText readRichText(const QTextDocument &document) {
    RichText out;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it)
            appendFragment(out, it.fragment());
        if (block.next().isValid()) {
            RichUnit lineBreak;
            lineBreak.ch = u'\n';
            out.push_back(lineBreak);
        }
    }
    return out;
}

void applyRichText(QTextDocument &document, const RichText &text) {
    const RichChange change = diffRichText(readRichText(document), text);
    if (change.empty()) {
        alignEquationBlocks(document);
        return;
    }
    QTextCursor cursor(&document);
    cursor.beginEditBlock();
    if (change.sameCharacters) {
        for (std::size_t i = change.start; i < change.start + change.added; ++i) {
            if (text[i].ch == u'\n')
                continue;
            cursor.setPosition(int(i));
            cursor.setPosition(int(i) + 1, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(styleFormat(text[i]));
        }
    } else {
        cursor.setPosition(int(change.start));
        cursor.setPosition(int(change.start + change.removed), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        insertUnits(cursor, text, change.start, change.start + change.added);
    }
    alignEquationBlocks(document);
    cursor.endEditBlock();
}

} // namespace peerchalk
