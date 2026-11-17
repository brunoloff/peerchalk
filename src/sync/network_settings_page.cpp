// Network: reading and saving the settings shown on the settings page.
#include "sync/network.hpp"
#include "sync/network_settings.hpp"
#include "ui/library.hpp"

#include <QVariantMap>

#include <exception>

namespace peerchalk::sync {

bool Network::settingsOverridden() { return NetworkSettings::environmentOverrides(); }

QVariantMap Network::settings() const {
    NetworkSettings saved;
    try {
        saved = NetworkSettings::load(library_.directory());
    } catch (const std::exception &error) {
        qWarning("PeerChalk shows default network settings: %s", error.what());
    }
    return {{QStringLiteral("enabled"), saved.enabled},
            {QStringLiteral("bootstrap"), saved.bootstrap},
            {QStringLiteral("dhtNetwork"), saved.dhtNetwork},
            {QStringLiteral("stun"), saved.stun},
            {QStringLiteral("turn"), saved.turn}};
}

QString Network::saveSettings(const QVariantMap &settings) {
    NetworkSettings chosen;
    chosen.enabled = settings.value(QStringLiteral("enabled"), true).toBool();
    chosen.bootstrap = settings.value(QStringLiteral("bootstrap")).toString();
    chosen.dhtNetwork = settings.value(QStringLiteral("dhtNetwork")).toString();
    chosen.stun = settings.value(QStringLiteral("stun")).toString();
    chosen.turn = settings.value(QStringLiteral("turn")).toString();
    try {
        chosen.save(library_.directory());
    } catch (const std::exception &error) {
        return QString::fromStdString(error.what());
    }
    restartNeeded_ = true;
    emit settingsChanged();
    return {};
}

} // namespace peerchalk::sync
