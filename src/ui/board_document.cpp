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

} // namespace peerchalk
