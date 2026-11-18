#pragma once

#include "board/rich_text.hpp"

#include <QTextFormat>

class QTextDocument;

namespace peerchalk {

// How equations appear in a QTextDocument: one object character with this type and these
// format properties.
inline constexpr int equationObjectType = QTextFormat::UserObject + 1;
inline constexpr int equationTexProperty = QTextFormat::UserProperty + 1;
inline constexpr int equationDisplayProperty = QTextFormat::UserProperty + 2;
// Changes to force a new layout; it says nothing about the equation.
inline constexpr int equationRevisionProperty = QTextFormat::UserProperty + 3;

// The document as rich text; positions match, with one unit per paragraph break.
[[nodiscard]] RichText readRichText(const QTextDocument &document);
// Makes the document show text through the smallest edit, so cursors elsewhere stay put,
// and centers the lines that hold only a display equation.
void applyRichText(QTextDocument &document, const RichText &text);

} // namespace peerchalk
