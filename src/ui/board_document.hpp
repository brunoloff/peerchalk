#pragma once

#include "board/board.hpp"

#include <QObject>
#include <QtQml/qqmlregistration.h>

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

  signals:
    void changed();

  private:
    Board board_;
    quint64 revision_{0};
    bool notifyQueued_{false};
};

} // namespace peerchalk
