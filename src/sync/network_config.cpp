#include "sync/network_config.hpp"

#include <QStringList>
#include <QUrl>

#include <stdexcept>
#include <string>

namespace peerchalk::sync {
namespace {

const QString defaultBootstrap = QStringLiteral("bootstrap.jami.net:4222");
// Free public STUN servers from Cloudflare, with Google's as a fallback.
const QString defaultStun = QStringLiteral("stun.cloudflare.com:3478, stun.l.google.com:19302");

const QString defaultDhtProxy = QStringLiteral("https://dhtproxy.jami.net");

struct HostPort {
    std::string host;
    std::uint16_t port;
};

HostPort hostPort(const QString &text, const char *setting) {
    const qsizetype colon = text.lastIndexOf(QLatin1Char(':'));
    bool ok = false;
    const uint port = colon > 0 ? text.mid(colon + 1).toUInt(&ok) : 0;
    if (!ok || port == 0 || port > 65535)
        throw std::invalid_argument(std::string(setting) + " must be host:port");
    return {text.left(colon).trimmed().toStdString(), static_cast<std::uint16_t>(port)};
}

p2pdc::TurnServer turnServer(const QString &turn) {
    const qsizetype at = turn.lastIndexOf(QLatin1Char('@'));
    const QStringList credentials = turn.left(at).split(QLatin1Char(':'));
    if (at <= 0 || credentials.size() != 2)
        throw std::invalid_argument("TURN server must be user:password@host:port");
    const HostPort server = hostPort(turn.mid(at + 1), "TURN server");
    return {server.host, server.port, credentials[0].toStdString(), credentials[1].toStdString()};
}

// Browsers only reach a proxy over HTTPS, or plain HTTP on this computer.
std::string dhtProxyUrl(const QString &text) {
    const QString value = text.trimmed().isEmpty() ? defaultDhtProxy : text.trimmed();
    const QUrl url(value, QUrl::StrictMode);
    const bool local =
        url.scheme() == QStringLiteral("http") &&
        (url.host() == QStringLiteral("localhost") || url.host() == QStringLiteral("127.0.0.1"));
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != QStringLiteral("https") && !local))
        throw std::invalid_argument("DHT proxy must be an https:// address");
    return value.toStdString();
}

} // namespace

NetworkConfig NetworkConfig::from(const NetworkSettings &settings) {
    NetworkConfig config;
    config.enabled = settings.enabled;
    const QString bootstrap =
        settings.bootstrap.trimmed().isEmpty() ? defaultBootstrap : settings.bootstrap;
    for (const QString &entry : bootstrap.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const HostPort server = hostPort(entry.trimmed(), "Bootstrap node");
        config.bootstrap.push_back({server.host, server.port});
    }
    if (!settings.dhtNetwork.trimmed().isEmpty()) {
        bool ok = false;
        config.networkId = settings.dhtNetwork.trimmed().toUInt(&ok);
        if (!ok)
            throw std::invalid_argument("DHT network must be a number");
    }
    const QString stun = settings.stun.trimmed().isEmpty() ? defaultStun : settings.stun;
    for (const QString &entry : stun.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const HostPort server = hostPort(entry.trimmed(), "STUN server");
        config.ice.servers.emplace_back(p2pdc::StunServer{server.host, server.port});
    }
    if (!settings.turn.trimmed().isEmpty())
        config.ice.servers.emplace_back(turnServer(settings.turn.trimmed()));
    config.dhtProxy = dhtProxyUrl(settings.dhtProxy);
    return config;
}

} // namespace peerchalk::sync
