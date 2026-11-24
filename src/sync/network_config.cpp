#include "sync/network_config.hpp"

#include "sync/network_defaults.hpp"

#include <QRegularExpression>
#include <QStringList>

#include <stdexcept>
#include <string>
#include <vector>

namespace peerchalk::sync {
namespace {

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

// One backend per line (or separated by ';'), in p2pdc's one-line form; blank lines and lines
// starting with '#' are skipped.
std::vector<p2pdc::BrowserBackend> discoveryBackends(const QString &text) {
    const QString list =
        text.trimmed().isEmpty() ? defaults::discovery().join(QLatin1Char('\n')) : text;
    std::vector<p2pdc::BrowserBackend> backends;
    for (const QString &line : list.split(QRegularExpression(QStringLiteral("[\\n;]")))) {
        const QString entry = line.trimmed();
        if (entry.isEmpty() || entry.startsWith(QLatin1Char('#')))
            continue;
        const auto backend = p2pdc::parse_browser_backend(entry.toStdString());
        if (!backend)
            throw std::invalid_argument(
                "Discovery servers: \"" + entry.toStdString() +
                "\" is not \"opendht URL...\", \"nostr URL\", \"mqtt URL\" or "
                "\"peerjs URL [key]\"");
        backends.push_back(*backend);
    }
    if (backends.empty() || backends.size() > p2pdc::max_browser_backends)
        throw std::invalid_argument("Discovery servers must list 1 to 8 servers");
    return backends;
}

} // namespace

NetworkConfig NetworkConfig::from(const NetworkSettings &settings) {
    NetworkConfig config;
    config.enabled = settings.enabled;
    const QString bootstrap = settings.bootstrap.trimmed().isEmpty()
                                  ? defaults::bootstrap().join(QLatin1Char(','))
                                  : settings.bootstrap;
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
    const QString stun =
        settings.stun.trimmed().isEmpty() ? defaults::stun().join(QLatin1Char(',')) : settings.stun;
    for (const QString &entry : stun.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const HostPort server = hostPort(entry.trimmed(), "STUN server");
        config.ice.servers.emplace_back(p2pdc::StunServer{server.host, server.port});
    }
    const QString turn = settings.turn.trimmed().isEmpty() ? defaults::turn().join(QLatin1Char(','))
                                                           : settings.turn.trimmed();
    if (turn.compare(defaults::noTurn(), Qt::CaseInsensitive) != 0) {
        for (const QString &entry : turn.split(QLatin1Char(','), Qt::SkipEmptyParts))
            config.ice.servers.emplace_back(turnServer(entry.trimmed()));
    }
    config.browserBackends = discoveryBackends(settings.discovery);
    return config;
}

} // namespace peerchalk::sync
