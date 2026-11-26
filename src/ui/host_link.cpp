#include "ui/host_link.hpp"

#include <QQmlEngine>

namespace peerchalk {

HostLink::HostLink(bool embedded, QObject *parent) : QObject(parent), embedded_(embedded) {}

HostLink *HostLink::create(QQmlEngine *engine, QJSEngine *) {
    return new HostLink(engine->property("hosted").toBool());
}

void HostLink::setBackLabel(const QString &label) {
    if (label == backLabel_)
        return;
    backLabel_ = label;
    emit backLabelChanged();
}

void HostLink::back() {
    if (!embedded_) {
        qWarning("PeerChalk: Back to the host, but no app embeds this one");
        return;
    }
    emit backRequested();
}

} // namespace peerchalk
