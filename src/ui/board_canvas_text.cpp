// Text boxes on BoardCanvas.
#include "ui/board_canvas.hpp"

#include "ui/conversions.hpp"
#include "ui/scene_nodes.hpp"

#include <QUuid>
#include <QtLogging>

#include <algorithm>

namespace peerchalk {
namespace {

constexpr float newBoxWidthPx = 320.0F;
constexpr float newBoxFontPx = 20.0F;

template <typename Change> void changeBox(Board &board, const QString &id, Change change) {
    const TextBox *found = board.textBox(id.toStdString());
    if (found == nullptr)
        return;
    TextBox box = *found;
    change(box);
    board.setTextBoxFrame(box);
}

} // namespace

QMatrix4x4 BoardCanvas::viewMatrix() const { return peerchalk::viewMatrix(viewport_); }

void BoardCanvas::textBoxesUpdated() {
    placedBoxes_ = placedTextBoxes();
    textBoxModel_.sync(placedBoxes_);
    if (!editingTextBox_.isEmpty() && board().textBox(editingTextBox_.toStdString()) == nullptr)
        setEditingTextBox({});
    emit textBoxesChanged();
}

void BoardCanvas::setEditingTextBox(const QString &id) {
    if (id == editingTextBox_)
        return;
    editingTextBox_ = id;
    emit editingTextBoxChanged();
}

void BoardCanvas::editTextBox(const QString &id) {
    if (!readOnly_ && board().textBox(id.toStdString()) != nullptr)
        setEditingTextBox(id);
}

void BoardCanvas::finishTextEditing() {
    if (editingTextBox_.isEmpty())
        return;
    const std::string id = editingTextBox_.toStdString();
    setEditingTextBox({});
    const TextBox *box = board().textBox(id);
    if (box != nullptr && box->body.empty())
        editableBoard().removeTextBox(id);
    editableBoard().endGesture();
    textBoxesUpdated();
}

const TextBox *BoardCanvas::textBoxAt(Point world) const {
    const std::vector<TextBox> &boxes = placedBoxes_;
    const auto hit = std::find_if(boxes.rbegin(), boxes.rend(), [&](const TextBox &box) {
        return board().textBoxBounds(box).contains(world);
    });
    return hit != boxes.rend() ? &*hit : nullptr;
}

void BoardCanvas::textToolPressed(QPointF position) {
    const Point world = viewport_.toWorld(toPoint(position));
    if (const TextBox *existing = textBoxAt(world)) {
        setEditingTextBox(QString::fromStdString(existing->id));
        return;
    }
    std::optional<TextBox> placed = newTextBoxAt(world);
    if (!placed)
        return;
    TextBox &box = *placed;
    box.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    box.width = std::clamp(newBoxWidthPx / viewport_.scale(), TextBox::minWidth, TextBox::maxWidth);
    box.fontSize =
        std::clamp(newBoxFontPx / viewport_.scale(), TextBox::minFontSize, TextBox::maxFontSize);
    box.y -= box.fontSize * 0.7F;
    box.argb = penColor_.rgba();
    editableBoard().addTextBox(box);
    textBoxesUpdated();
    setEditingTextBox(QString::fromStdString(box.id));
}

void BoardCanvas::setTextBoxBody(const QString &id, const RichText &body) {
    if (!readOnly_)
        editableBoard().setTextBoxBody(id.toStdString(), body);
}

void BoardCanvas::setTextBoxHeight(const QString &id, qreal height) {
    editableBoard().setTextBoxHeight(id.toStdString(), float(height));
}

void BoardCanvas::endTextBoxChange() { editableBoard().endGesture(); }

void BoardCanvas::moveTextBox(const QString &id, QPointF position) {
    if (readOnly_ || !std::isfinite(position.x()) || !std::isfinite(position.y()))
        return;
    changeBox(editableBoard(), id, [&](TextBox &box) {
        const Point origin = boxOrigin(box);
        box.x = float(position.x()) - origin.x;
        box.y = float(position.y()) - origin.y;
    });
    textBoxesUpdated();
}

void BoardCanvas::setTextBoxWidth(const QString &id, qreal width) {
    if (readOnly_ || !std::isfinite(width))
        return;
    changeBox(editableBoard(), id, [&](TextBox &box) {
        box.width = std::clamp(float(width), TextBox::minWidth, TextBox::maxWidth);
    });
    textBoxesUpdated();
}

void BoardCanvas::setTextBoxFontSize(const QString &id, qreal size) {
    if (readOnly_ || !std::isfinite(size))
        return;
    changeBox(editableBoard(), id, [&](TextBox &box) {
        box.fontSize = std::clamp(float(size), TextBox::minFontSize, TextBox::maxFontSize);
    });
    editableBoard().endGesture();
    textBoxesUpdated();
}

void BoardCanvas::setTextBoxColor(const QString &id, const QColor &color) {
    if (readOnly_ || !color.isValid()) {
        qWarning("BoardCanvas: ignoring text colour change");
        return;
    }
    changeBox(editableBoard(), id, [&](TextBox &box) { box.argb = color.rgba(); });
    editableBoard().endGesture();
    textBoxesUpdated();
}

void BoardCanvas::removeTextBox(const QString &id) {
    if (readOnly_)
        return;
    editableBoard().removeTextBox(id.toStdString());
    editableBoard().endGesture();
    textBoxesUpdated();
}

} // namespace peerchalk
