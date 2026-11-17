#include "sync/network_config.hpp"

#include <QStringList>

#include <stdexcept>

namespace peerchalk::sync {
namespace {

struct HostPort {
    std::string host;
    std::uint16_t port;
};

HostPort hostPort(const QString &text, const char *variable) {
    const qsizetype colon = text.lastIndexOf(QLatin1Char(':'));
    bool ok = false;
    const uint port = colon > 0 ? text.mid(colon + 1).toUInt(&ok) : 0;
    if (!ok || port == 0 || port > 65535)
        throw std::invalid_argument(std::string(variable) + " must be host:port");
    return {text.left(colon).trimmed().toStdString(), static_cast<std::uint16_t>(port)};
}

} // namespace

NetworkConfig NetworkConfig::fromEnvironment() {
    NetworkConfig config;
    config.enabled = qEnvironmentVariable("PEERCHALK_NETWORK") != QStringLiteral("off");
    const QString bootstrap =
        qEnvironmentVariable("PEERCHALK_BOOTSTRAP", QStringLiteral("bootstrap.jami.net:4222"));
    for (const QString &entry : bootstrap.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const HostPort server = hostPort(entry.trimmed(), "PEERCHALK_BOOTSTRAP");
        config.bootstrap.push_back({server.host, server.port});
    }
    if (qEnvironmentVariableIsSet("PEERCHALK_DHT_NETWORK")) {
        bool ok = false;
        config.networkId = qEnvironmentVariable("PEERCHALK_DHT_NETWORK").toUInt(&ok);
        if (!ok)
            throw std::invalid_argument("PEERCHALK_DHT_NETWORK must be a number");
    }
    if (qEnvironmentVariableIsSet("PEERCHALK_STUN")) {
        const HostPort stun = hostPort(qEnvironmentVariable("PEERCHALK_STUN"), "PEERCHALK_STUN");
        config.ice.servers.emplace_back(p2pdc::StunServer{stun.host, stun.port});
    }
    if (qEnvironmentVariableIsSet("PEERCHALK_TURN")) {
        const QString turn = qEnvironmentVariable("PEERCHALK_TURN");
        const qsizetype at = turn.lastIndexOf(QLatin1Char('@'));
        const QStringList credentials = turn.left(at).split(QLatin1Char(':'));
        if (at <= 0 || credentials.size() != 2)
            throw std::invalid_argument("PEERCHALK_TURN must be user:password@host:port");
        const HostPort server = hostPort(turn.mid(at + 1), "PEERCHALK_TURN");
        config.ice.servers.emplace_back(p2pdc::TurnServer{
            server.host, server.port, credentials[0].toStdString(), credentials[1].toStdString()});
    }
    return config;
}

} // namespace peerchalk::sync
