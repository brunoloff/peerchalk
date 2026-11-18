#include "ui/board_document.hpp"

namespace peerchalk {

BoardDocument::BoardDocument(QObject *parent) : QObject(parent) {
    // The listener runs inside the Yrs commit, so notification waits for the event loop;
    // several changes in one turn produce one notification.
    board_.setChangeListener([this] {
        ++revision_;
        if (notifyQueued_)
            return;
        notifyQueued_ = true;
        QMetaObject::invokeMethod(
            this,
            [this] {
                notifyQueued_ = false;
                emit changed();
            },
            Qt::QueuedConnection);
    });
}

void BoardDocument::publishDraft(const Stroke &stroke, DraftState state) const {
    if (draftListener_)
        draftListener_(stroke, state);
}

void BoardDocument::setRemoteDraft(const QString &key, const Stroke &stroke) {
    remoteDrafts_.insert_or_assign(key, stroke);
    emit remoteDraftsChanged();
}

void BoardDocument::removeRemoteDraft(const QString &key) {
    if (remoteDrafts_.erase(key) > 0)
        emit remoteDraftsChanged();
}

} // namespace peerchalk
