#pragma once

#include "board/board.hpp"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <map>

namespace peerchalk {

// A board's live Yrs document, shared by everything showing or syncing that board.
// `changed` is emitted on the next event-loop turn after the strokes change, locally or
// from a peer, so receivers may freely read and modify the board.
class BoardDocument : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Opened through Library.openBoard")

  public:
    explicit BoardDocument(QObject *parent = nullptr);

    [[nodiscard]] Board &board() { return board_; }
    [[nodiscard]] const Board &board() const { return board_; }
    // Counts changes as they happen (before `changed` is emitted), so savers can tell
    // whether anything changed since they last saved.
    [[nodiscard]] quint64 revision() const { return revision_; }

    // Strokes still being drawn. They are not part of the board: the canvas reports
    // yours to the listener as you draw, and other people's are kept here for display.
    using DraftListener = std::function<void(const Stroke &, DraftState)>;
    void setDraftListener(DraftListener listener) { draftListener_ = std::move(listener); }
    void publishDraft(const Stroke &stroke, DraftState state) const;
    [[nodiscard]] const std::map<QString, Stroke> &remoteDrafts() const { return remoteDrafts_; }
    void setRemoteDraft(const QString &key, const Stroke &stroke);
    void removeRemoteDraft(const QString &key);

  signals:
    void changed();
    void remoteDraftsChanged();

  private:
    Board board_;
    quint64 revision_{0};
    bool notifyQueued_{false};
    DraftListener draftListener_;
    std::map<QString, Stroke> remoteDrafts_;
};

} // namespace peerchalk
