// Network: reading and saving the settings shown on the settings page.
#include "sync/network.hpp"
#include "sync/network_settings.hpp"
#include "sync/server_list.hpp"
#include "ui/library.hpp"

#include <QVariantMap>

#include <exception>
#include <functional>

namespace peerchalk::sync {
namespace {

constexpr bool inBrowser =
#ifdef __EMSCRIPTEN__
    true;
#else
    false;
#endif

ServerGroup groupNamed(const QString &group) {
    return group == QStringLiteral("firewall") ? ServerGroup::Firewall : ServerGroup::Discovery;
}

NetworkSettings savedSettings(const QString &directory) {
    try {
        return NetworkSettings::load(directory);
    } catch (const std::exception &error) {
        qWarning("PeerChalk shows default network settings: %s", error.what());
        return {};
    }
}

} // namespace

bool Network::settingsOverridden() { return NetworkSettings::environmentOverrides(); }

QVariantMap Network::settings() const {
    const NetworkSettings saved = savedSettings(library_.directory());
    return {{QStringLiteral("enabled"), saved.enabled},
            {QStringLiteral("bootstrap"), saved.bootstrap},
            {QStringLiteral("dhtNetwork"), saved.dhtNetwork},
            {QStringLiteral("stun"), saved.stun},
            {QStringLiteral("turn"), saved.turn},
            {QStringLiteral("discovery"), saved.discovery}};
}

QString Network::saveSettings(const QVariantMap &settings) {
    NetworkSettings chosen = savedSettings(library_.directory());
    chosen.enabled = settings.value(QStringLiteral("enabled"), chosen.enabled).toBool();
    chosen.dhtNetwork = settings.value(QStringLiteral("dhtNetwork"), chosen.dhtNetwork).toString();
    try {
        chosen.save(library_.directory());
    } catch (const std::exception &error) {
        return QString::fromStdString(error.what());
    }
    restartNeeded_ = true;
    emit settingsChanged();
    return {};
}

QVariantList Network::servers(const QString &group) const {
    const ServerList list(savedSettings(library_.directory()), inBrowser);
    QVariantList rows;
    for (const ServerEntry &entry : list.entries(groupNamed(group)))
        rows.push_back(
            QVariantMap{{QStringLiteral("kind"), QString::fromLatin1(serverKindName(entry.kind))},
                        {QStringLiteral("address"), entry.address},
                        {QStringLiteral("isDefault"), entry.isDefault}});
    return rows;
}

QString Network::changeServers(const std::function<void(ServerList &)> &change) {
    try {
        ServerList list(savedSettings(library_.directory()), inBrowser);
        change(list);
        list.settings().save(library_.directory());
    } catch (const std::exception &error) {
        return QString::fromStdString(error.what());
    }
    restartNeeded_ = true;
    emit settingsChanged();
    return {};
}

QString Network::addServer(const QString &kind, const QString &address) {
    return changeServers([&](ServerList &list) { list.add(serverKindNamed(kind), address); });
}

QString Network::removeServer(const QString &group, int index) {
    return changeServers(
        [&](ServerList &list) { list.remove(groupNamed(group), static_cast<std::size_t>(index)); });
}

QString Network::resetServers(const QString &group) {
    return changeServers([&](ServerList &list) { list.reset(groupNamed(group)); });
}

} // namespace peerchalk::sync
