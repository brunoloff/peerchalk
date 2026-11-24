// Network: what the network page shows about connections and servers.
#include "sync/network.hpp"
#include "sync/network_settings.hpp"
#include "sync/p2pdc_names.hpp"
#include "sync/server_checks.hpp"
#include "sync/server_list.hpp"
#include "ui/library.hpp"

#include <algorithm>
#include <map>

namespace peerchalk::sync {
namespace {

constexpr bool inBrowser =
#ifdef __EMSCRIPTEN__
    true;
#else
    false;
#endif

// How good a way to reach a device is; a device in several spaces shows its best.
int rank(const QString &route) {
    return route == QStringLiteral("direct")    ? 3
           : route == QStringLiteral("relay")   ? 2
           : route == QStringLiteral("unknown") ? 1
                                                : 0;
}

QString routeName(bool ready, p2pdc::RouteKind kind) {
    if (!ready)
        return QStringLiteral("connecting");
    return kind == p2pdc::RouteKind::Direct        ? QStringLiteral("direct")
           : kind == p2pdc::RouteKind::TurnRelayed ? QStringLiteral("relay")
                                                   : QStringLiteral("unknown");
}

// Milliseconds, or nothing (undefined in QML) until p2pdc has measured the round trip.
QVariant milliseconds(std::optional<std::chrono::milliseconds> value) {
    return value ? QVariant(static_cast<int>(value->count())) : QVariant();
}

// Whether candidate is a better way to reach a device than existing: a better route, then
// a shorter round trip.
bool better(const QVariantMap &candidate, const QVariantMap &existing) {
    const int was = rank(existing.value(QStringLiteral("route")).toString());
    const int is = rank(candidate.value(QStringLiteral("route")).toString());
    if (is != was)
        return is > was;
    const QVariant old = existing.value(QStringLiteral("milliseconds"));
    const QVariant now = candidate.value(QStringLiteral("milliseconds"));
    return now.isValid() && (!old.isValid() || now.toInt() < old.toInt());
}

} // namespace

QVariantList Network::devices() const {
    std::map<p2pdc::SigningPublicKey, QVariantMap> byDevice;
    for (const auto &[id, peer] : peers_) {
        const bool ready = peer->ready && peer->user;
        QVariantMap candidate{
            {QStringLiteral("device"), shortDevice(peer->device)},
            {QStringLiteral("name"), peer->user ? peer->user->name : QString()},
            {QStringLiteral("colorIndex"), peer->user ? peer->user->colorIndex : 0},
            {QStringLiteral("route"), routeName(ready, peer->routeKind)},
            {QStringLiteral("milliseconds"), milliseconds(ready ? peer->roundTrip : std::nullopt)}};
        QVariantMap &entry = byDevice[peer->device];
        if (entry.isEmpty() || better(candidate, entry))
            entry = std::move(candidate);
    }
    QVariantList list;
    for (auto &[device, entry] : byDevice)
        list.push_back(std::move(entry));
    std::ranges::sort(list, [](const QVariant &a, const QVariant &b) {
        return a.toMap()
                   .value(QStringLiteral("name"))
                   .toString()
                   .localeAwareCompare(b.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
    return list;
}

void Network::checkServers() {
    if (serverChecks_ == nullptr) {
        serverChecks_ = new ServerChecks(this);
        connect(serverChecks_, &ServerChecks::checked, this,
                [this](const QString &key, bool working, int milliseconds) {
                    serverStatus_[key] = QVariantMap{
                        {QStringLiteral("state"),
                         working ? QStringLiteral("working") : QStringLiteral("unreachable")},
                        {QStringLiteral("milliseconds"), milliseconds}};
                    emit serverStatusChanged();
                });
    }
    NetworkSettings saved;
    try {
        saved = NetworkSettings::load(library_.directory()).withEnvironment();
    } catch (const std::exception &error) {
        qWarning("PeerChalk checks the default servers: %s", error.what());
    }
    const ServerList list(saved, inBrowser);
    std::vector<ServerTarget> targets;
    for (const ServerGroup group : {ServerGroup::Discovery, ServerGroup::Firewall}) {
        for (const ServerEntry &entry : list.entries(group)) {
            const QString key =
                QLatin1String(serverKindName(entry.kind)) + QLatin1Char(' ') + entry.address;
            targets.push_back({key, entry.kind, entry.address, entry.login});
            serverStatus_[key] = QVariantMap{{QStringLiteral("state"), QStringLiteral("checking")}};
        }
    }
    emit serverStatusChanged();
    serverChecks_->check(targets);
}

} // namespace peerchalk::sync
