#pragma once

#include "library/instance_slot.hpp"

#include <QByteArray>
#include <QObject>

#include <memory>

namespace peerchalk::sync {

// Carries messages directly between the app instances on this device, so that they stay
// in step at once and even offline: a BroadcastChannel in browsers, local sockets on the
// desktop.
class InstanceLink : public QObject {
    Q_OBJECT

  public:
    [[nodiscard]] static std::unique_ptr<InstanceLink> create(const InstanceSlot &slot);

    // Delivers the message to every other instance that is running.
    virtual void send(const QByteArray &message) = 0;

  signals:
    void received(const QByteArray &message);
    // Another instance can now be reached and may have missed earlier messages.
    void instanceJoined();

  protected:
    using QObject::QObject;
};

} // namespace peerchalk::sync
