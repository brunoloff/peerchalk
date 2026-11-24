#include "sync/network_config.hpp"

#include <QStringList>
#include <QUrl>

#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace peerchalk::sync {
namespace {

// Jami's node, then PeerChalk's own on the relay's server (deploy/dht-proxy/).
const QString defaultBootstrap = QStringLiteral("bootstrap.jami.net:4222, 89.168.35.4:4222");
// Free public STUN servers from Cloudflare, with Google's as a fallback.
const QString defaultStun = QStringLiteral("stun.cloudflare.com:3478, stun.l.google.com:19302");

// PeerChalk's open relay (deploy/coturn/), limited per IP address; its login is meant to be
// shared, like a public STUN server.
const QString defaultTurn = QStringLiteral("peerchalk:peerchalk-open-relay@89.168.35.4:3478");
const QString noTurn = QStringLiteral("none");

// Jami's proxy, then PeerChalk's own: Jami's has resolved to a dead address in parts of Asia.
const QString defaultDhtProxies =
    QStringLiteral("https://dhtproxy.jami.net, https://89-168-35-4.sslip.io");
constexpr qsizetype maxDhtProxies = 8; // as p2pdc allows

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
std::string dhtProxyUrl(const QString &value) {
    const QUrl url(value, QUrl::StrictMode);
    const bool local =
        url.scheme() == QStringLiteral("http") &&
        (url.host() == QStringLiteral("localhost") || url.host() == QStringLiteral("127.0.0.1"));
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != QStringLiteral("https") && !local))
        throw std::invalid_argument("DHT proxy must be an https:// address");
    return value.toStdString();
}

// Comma-separated proxies, tried in order.
std::vector<std::string> dhtProxyUrls(const QString &text) {
    const QString list = text.trimmed().isEmpty() ? defaultDhtProxies : text;
    std::vector<std::string> urls;
    for (const QString &entry : list.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        if (!entry.trimmed().isEmpty())
            urls.push_back(dhtProxyUrl(entry.trimmed()));
    }
    if (urls.empty() || std::ssize(urls) > maxDhtProxies)
        throw std::invalid_argument("DHT proxy must list 1 to 8 https:// addresses");
    return urls;
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
    const QString turn = settings.turn.trimmed().isEmpty() ? defaultTurn : settings.turn.trimmed();
    if (turn.compare(noTurn, Qt::CaseInsensitive) != 0)
        config.ice.servers.emplace_back(turnServer(turn));
    config.dhtProxies = dhtProxyUrls(settings.dhtProxy);
    return config;
}

} // namespace peerchalk::sync
