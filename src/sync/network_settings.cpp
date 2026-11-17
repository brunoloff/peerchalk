#include "sync/network_settings.hpp"

#include "library/files.hpp"
#include "sync/network_config.hpp"

#include <QDir>
#include <QJsonObject>

#include <array>

namespace peerchalk::sync {
namespace {

constexpr int fileVersion = 1;

QString settingsPath(const QString &directory) {
    return QDir(directory).filePath(QStringLiteral("network.json"));
}

void override(QString &field, const char *variable) {
    if (qEnvironmentVariableIsSet(variable))
        field = qEnvironmentVariable(variable);
}

} // namespace

NetworkSettings NetworkSettings::load(const QString &directory) {
    const std::optional<QJsonObject> root = readJsonObject(settingsPath(directory), fileVersion);
    if (!root)
        return {};
    NetworkSettings settings;
    settings.enabled = root->value(QStringLiteral("enabled")).toBool(true);
    settings.bootstrap = root->value(QStringLiteral("bootstrap")).toString();
    settings.dhtNetwork = root->value(QStringLiteral("dhtNetwork")).toString();
    settings.stun = root->value(QStringLiteral("stun")).toString();
    settings.turn = root->value(QStringLiteral("turn")).toString();
    return settings;
}

void NetworkSettings::save(const QString &directory) const {
    static_cast<void>(NetworkConfig::from(*this)); // throws when a field is malformed
    const QJsonObject root{{QStringLiteral("enabled"), enabled},
                           {QStringLiteral("bootstrap"), bootstrap.trimmed()},
                           {QStringLiteral("dhtNetwork"), dhtNetwork.trimmed()},
                           {QStringLiteral("stun"), stun.trimmed()},
                           {QStringLiteral("turn"), turn.trimmed()}};
    writeJsonObject(settingsPath(directory), root, fileVersion, true);
}

NetworkSettings NetworkSettings::withEnvironment() const {
    NetworkSettings result = *this;
    if (qEnvironmentVariableIsSet("PEERCHALK_NETWORK"))
        result.enabled = qEnvironmentVariable("PEERCHALK_NETWORK") != QStringLiteral("off");
    override(result.bootstrap, "PEERCHALK_BOOTSTRAP");
    override(result.dhtNetwork, "PEERCHALK_DHT_NETWORK");
    override(result.stun, "PEERCHALK_STUN");
    override(result.turn, "PEERCHALK_TURN");
    return result;
}

bool NetworkSettings::environmentOverrides() {
    constexpr std::array variables = {"PEERCHALK_NETWORK", "PEERCHALK_BOOTSTRAP",
                                      "PEERCHALK_DHT_NETWORK", "PEERCHALK_STUN", "PEERCHALK_TURN"};
    for (const char *variable : variables)
        if (qEnvironmentVariableIsSet(variable))
            return true;
    return false;
}

} // namespace peerchalk::sync
