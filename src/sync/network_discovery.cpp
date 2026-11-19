// Network: watching and announcing spaces, and answering devices that call in. Every step is
// written to the connection log for troubleshooting.
#include "sync/connection_log.hpp"
#include "sync/network.hpp"
#include "sync/p2pdc_names.hpp"

#include <QTimer>

namespace peerchalk::sync {

using namespace p2pdc;

namespace {

const QString discoveryArea = QStringLiteral("discovery");

} // namespace

void Network::peerAdvertised(const QString &key, Advertisement value, PeerEvent event) {
    const auto it = spaces_.find(key);
    if (it == spaces_.end() || isOwnDevice(value.signing_public_key))
        return;
    logInfo(discoveryArea, QStringLiteral("%1: device %2 %3")
                               .arg(spaceLabel(key), shortDevice(value.signing_public_key),
                                    describe(event)));
    auto &known = it->second->advertisements;
    if (event == PeerEvent::Expired) {
        const auto found = known.find(value.signing_public_key);
        if (found != known.end() && found->second.fields.instance_id == value.fields.instance_id)
            known.erase(found);
        return;
    }
    known.insert_or_assign(value.signing_public_key, std::move(value));
    tryConnect(key);
}

DiscoveryHandlers Network::discoveryHandlers(const QString &key) {
    DiscoveryHandlers callbacks{
        [this, key](Advertisement value, PeerEvent event) {
            peerAdvertised(key, std::move(value), event);
        },
        [this, key](bool ok) {
            logInfo(discoveryArea, QStringLiteral("%1: lookup %2")
                                       .arg(spaceLabel(key), ok ? QStringLiteral("finished")
                                                                : QStringLiteral("incomplete")));
            if (const auto it = spaces_.find(key); it != spaces_.end() && ok) {
                it->second->caught = true;
                tryConnect(key);
            }
        },
        [this, key](WatchError error) {
            logWarning(discoveryArea,
                       QStringLiteral("%1: watching failed: %2").arg(spaceLabel(key), describe(error)));
            // Reopened by the next reconcile.
            QTimer::singleShot(0, this, [this, key] { closeSpace(key); });
        }};
    callbacks.on_status = [key](DiscoveryStatus status) {
        logInfo(discoveryArea,
                QStringLiteral("%1: discovery %2").arg(spaceLabel(key), describe(status)));
    };
    return callbacks;
}

bool Network::watch(const QString &key) {
    Space &space = *spaces_.at(key);
    auto result = node_->watch(space.capability, scheduler(), {}, discoveryHandlers(key));
    auto *subscription = std::get_if<std::unique_ptr<Subscription>>(&result);
    if (subscription == nullptr) {
        logWarning(discoveryArea, QStringLiteral("%1: could not watch: %2")
                                      .arg(spaceLabel(key), describe(std::get<WatchError>(result))));
        return false;
    }
    space.watch = std::move(*subscription);
    return true;
}

bool Network::announce(const QString &key) {
    Space &space = *spaces_.at(key);
    AnnounceHandlers callbacks{
        [this, key] {
            logInfo(discoveryArea, QStringLiteral("%1: announced").arg(spaceLabel(key)));
            if (const auto it = spaces_.find(key); it != spaces_.end()) {
                it->second->published = true;
                tryConnect(key);
            }
        },
        [this, key](AnnouncementError error) {
            logWarning(discoveryArea, QStringLiteral("%1: announcing failed: %2")
                                          .arg(spaceLabel(key), describe(error)));
            if (error != AnnouncementError::Retrying)
                QTimer::singleShot(0, this, [this, key] { closeSpace(key); });
        }};
    auto result = node_->announce(space.capability, scheduler(), {}, std::move(callbacks));
    auto *lease = std::get_if<std::unique_ptr<Lease>>(&result);
    if (lease == nullptr) {
        logWarning(discoveryArea,
                   QStringLiteral("%1: could not announce: %2")
                       .arg(spaceLabel(key), describe(std::get<AnnouncementError>(result))));
        return false;
    }
    space.lease = std::move(*lease);
    return true;
}

bool Network::listen(const QString &key) {
    Space &space = *spaces_.at(key);
    IncomingOptions options;
    options.persistent = true;
    IncomingHandlers callbacks;
    callbacks.on_error = [this, key](IncomingError error) {
        logWarning(QStringLiteral("incoming"), QStringLiteral("%1: listening failed: %2")
                                                   .arg(spaceLabel(key), describe(error)));
        QTimer::singleShot(0, this, [this, key] {
            if (const auto it = spaces_.find(key); it != spaces_.end())
                it->second->listener.reset(); // recreated by the next tryConnect
        });
    };
    callbacks.on_request_identified = [this, key](IncomingRequestId id, Advertisement peer) {
        request(key, id, std::move(peer));
    };
    callbacks.on_overflow = [key] {
        logWarning(QStringLiteral("incoming"),
                   QStringLiteral("%1: too many calls at once; some were dropped").arg(spaceLabel(key)));
    };
    auto result =
        node_->listen(*space.lease, *space.watch, scheduler(), options, std::move(callbacks));
    auto *listener = std::get_if<std::unique_ptr<IncomingListener>>(&result);
    if (listener == nullptr) {
        logWarning(QStringLiteral("incoming"),
                   QStringLiteral("%1: could not listen: %2")
                       .arg(spaceLabel(key), describe(std::get<IncomingError>(result))));
        return false;
    }
    space.listener = std::move(*listener);
    return true;
}

// The device with the lower key dials; the other accepts.
void Network::tryConnect(const QString &key) {
    const auto it = spaces_.find(key);
    if (it == spaces_.end() || stopping_ || !node_ || !node_->identity())
        return;
    Space &space = *it->second;
    if (!space.published || !space.caught || !space.watch || !space.lease)
        return;
    if (!space.listener && !listen(key))
        return;
    const SigningPublicKey own = node_->identity()->signing_key;
    for (const auto &[device, advertisement] : space.advertisements) {
        const auto peer = peers_.find({key, device});
        const bool connected = peer != peers_.end() && peer->second->connection;
        if (own < device && !connected && !isIncompatible(device) &&
            space.watch->is_current(advertisement))
            connectPeer(key, advertisement);
    }
}

// Why a call from this device is turned away, or an empty string to accept it.
QString Network::refusal(const Space &space, const PeerId &id, const Advertisement &advertisement) {
    const auto existing = peers_.find(id);
    if (!node_->identity())
        return QStringLiteral("this device has no identity yet");
    if (!space.watch->is_current(advertisement))
        return QStringLiteral("its announcement is out of date");
    if (id.second >= node_->identity()->signing_key)
        return QStringLiteral("this device dials it instead");
    if (existing != peers_.end() && existing->second->connection)
        return QStringLiteral("already connected or connecting");
    if (isIncompatible(id.second))
        return QStringLiteral("incompatible version");
    return {};
}

void Network::request(const QString &key, IncomingRequestId id, Advertisement advertisement) {
    const auto it = spaces_.find(key);
    if (it == spaces_.end())
        return;
    const PeerId peerId{key, advertisement.signing_public_key};
    const QString who = QStringLiteral("%1: call from %2").arg(spaceLabel(key),
                                                               shortDevice(peerId.second));
    if (const QString reason = refusal(*it->second, peerId, advertisement); !reason.isEmpty()) {
        logInfo(QStringLiteral("incoming"), QStringLiteral("%1 declined: %2").arg(who, reason));
        static_cast<void>(it->second->listener->reject(id));
        return;
    }
    const auto generation = ++nextGeneration_;
    auto result = node_->accept(*it->second->listener, id, config_.ice, scheduler(),
                                connectOptions(), handlers(peerId, generation));
    auto *connection = std::get_if<std::unique_ptr<Connection>>(&result);
    if (connection == nullptr) {
        logWarning(QStringLiteral("incoming"), QStringLiteral("%1 could not be accepted: %2")
                                                   .arg(who, describe(std::get<ConnectError>(result))));
        static_cast<void>(it->second->listener->reject(id));
        return;
    }
    logInfo(QStringLiteral("incoming"), QStringLiteral("%1 accepted").arg(who));
    addPeer(peerId, generation, std::move(*connection));
}

} // namespace peerchalk::sync
