#include "sync/server_list.hpp"

#include "sync/network_config.hpp"
#include "sync/network_defaults.hpp"

#include <QRegularExpression>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace peerchalk::sync {
namespace {

const QString proxyPrefix = QStringLiteral("opendht ");

// The entries of a list setting, or the fallback when the setting is empty.
QStringList listOf(const QString &text, const QStringList &fallback,
                   const QRegularExpression &sep) {
    QStringList list;
    for (const QString &part : text.split(sep)) {
        const QString entry = part.trimmed();
        if (!entry.isEmpty() && !entry.startsWith(QLatin1Char('#')))
            list.push_back(entry);
    }
    return list.isEmpty() ? fallback : list;
}

QRegularExpression commas() { return QRegularExpression(QStringLiteral(",")); }
QRegularExpression lines() { return QRegularExpression(QStringLiteral("[\\n;]")); }

// How a list is saved: empty while it equals the defaults.
QString stored(const QStringList &list, const QStringList &fallback, const QString &sep) {
    return list == fallback ? QString() : list.join(sep);
}

QString turnAddress(const QString &turn) {
    return turn.mid(turn.lastIndexOf(QLatin1Char('@')) + 1);
}

QString kindName(ServerKind kind) {
    switch (kind) {
    case ServerKind::Nostr:
        return QStringLiteral("nostr");
    case ServerKind::Mqtt:
        return QStringLiteral("mqtt");
    case ServerKind::PeerJs:
        return QStringLiteral("peerjs");
    default:
        return QStringLiteral("opendht");
    }
}

ServerKind kindOf(const QString &line) {
    if (line.startsWith(QStringLiteral("nostr ")))
        return ServerKind::Nostr;
    if (line.startsWith(QStringLiteral("mqtt ")))
        return ServerKind::Mqtt;
    return ServerKind::PeerJs;
}

QStringList proxiesIn(const QStringList &discovery) {
    for (const QString &line : discovery) {
        if (line.startsWith(proxyPrefix))
            return line.mid(proxyPrefix.size()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    }
    return {};
}

void refuseDuplicate(const QStringList &list, const QString &entry) {
    if (list.contains(entry))
        throw std::invalid_argument("That server is already in the list.");
}

void refuseEmptying(const QStringList &list, const char *message) {
    if (list.size() <= 1)
        throw std::invalid_argument(message);
}

constexpr const char *kindNames[] = {"dht", "nostr", "mqtt", "peerjs", "stun", "turn"};

} // namespace

const char *serverKindName(ServerKind kind) { return kindNames[static_cast<int>(kind)]; }

ServerKind serverKindNamed(const QString &name) {
    for (int kind = 0; kind <= static_cast<int>(ServerKind::Turn); ++kind) {
        if (name == QLatin1String(kindNames[kind]))
            return static_cast<ServerKind>(kind);
    }
    throw std::invalid_argument("Unknown kind of server: " + name.toStdString());
}

ServerList::ServerList(NetworkSettings settings, bool browser)
    : settings_(std::move(settings)), browser_(browser) {}

std::vector<ServerEntry> ServerList::entries(ServerGroup group) const {
    return group == ServerGroup::Discovery ? discoveryEntries() : firewallEntries();
}

QStringList ServerList::discoveryLines() const {
    return listOf(settings_.discovery, defaults::discovery(), lines());
}

void ServerList::setDiscoveryLines(const QStringList &list) {
    NetworkSettings candidate = settings_;
    candidate.discovery = stored(list, defaults::discovery(), QStringLiteral("\n"));
    commit(candidate);
}

QStringList ServerList::proxies() const { return proxiesIn(discoveryLines()); }

std::vector<ServerEntry> ServerList::discoveryEntries() const {
    std::vector<ServerEntry> rows;
    const QStringList dht =
        browser_ ? proxies() : listOf(settings_.bootstrap, defaults::bootstrap(), commas());
    const QStringList defaultDht =
        browser_ ? proxiesIn(defaults::discovery()) : defaults::bootstrap();
    for (const QString &node : dht)
        rows.push_back({ServerKind::Dht, node, defaultDht.contains(node), false, {}});
    for (const QString &line : discoveryLines()) {
        if (line.startsWith(proxyPrefix))
            continue;
        const QString address = line.section(QLatin1Char(' '), 1).trimmed();
        rows.push_back(
            {kindOf(line), address, defaults::discovery().contains(line), !browser_, {}});
    }
    return rows;
}

std::vector<ServerEntry> ServerList::firewallEntries() const {
    std::vector<ServerEntry> rows;
    for (const QString &server : listOf(settings_.stun, defaults::stun(), commas()))
        rows.push_back({ServerKind::Stun, server, defaults::stun().contains(server), false, {}});
    if (settings_.turn.trimmed().compare(defaults::noTurn(), Qt::CaseInsensitive) == 0)
        return rows;
    for (const QString &server : listOf(settings_.turn, defaults::turn(), commas()))
        rows.push_back({ServerKind::Turn, turnAddress(server), defaults::turn().contains(server),
                        false, server.left(server.lastIndexOf(QLatin1Char('@')))});
    return rows;
}

void ServerList::add(ServerKind kind, const QString &address) {
    const QString entry = address.trimmed();
    if (entry.isEmpty())
        throw std::invalid_argument("Type the server's address.");
    NetworkSettings candidate = settings_;
    if (kind == ServerKind::Stun || kind == ServerKind::Turn) {
        const bool stun = kind == ServerKind::Stun;
        QStringList list = listOf(stun ? settings_.stun : settings_.turn,
                                  stun ? defaults::stun() : defaults::turn(), commas());
        if (!stun && settings_.turn.trimmed().compare(defaults::noTurn(), Qt::CaseInsensitive) == 0)
            list.clear();
        refuseDuplicate(list, entry);
        list.push_back(entry);
        (stun ? candidate.stun : candidate.turn) =
            stored(list, stun ? defaults::stun() : defaults::turn(), QStringLiteral(", "));
        commit(candidate);
        return;
    }
    addDiscovery(kind, entry);
}

void ServerList::addDiscovery(ServerKind kind, const QString &entry) {
    QStringList list = discoveryLines();
    if (kind == ServerKind::Dht && !browser_) {
        QStringList nodes = listOf(settings_.bootstrap, defaults::bootstrap(), commas());
        refuseDuplicate(nodes, entry);
        nodes.push_back(entry);
        NetworkSettings candidate = settings_;
        candidate.bootstrap = stored(nodes, defaults::bootstrap(), QStringLiteral(", "));
        commit(candidate);
        return;
    }
    if (kind == ServerKind::Dht) {
        QStringList urls = proxies();
        refuseDuplicate(urls, entry);
        urls.push_back(entry);
        list.removeIf([](const QString &line) { return line.startsWith(proxyPrefix); });
        list.prepend(proxyPrefix + urls.join(QLatin1Char(' ')));
    } else {
        const QString line = kindName(kind) + QLatin1Char(' ') + entry;
        refuseDuplicate(list, line);
        list.push_back(line);
    }
    setDiscoveryLines(list);
}

void ServerList::remove(ServerGroup group, std::size_t index) {
    const std::vector<ServerEntry> rows = entries(group);
    if (index >= rows.size())
        throw std::out_of_range("No server at that place in the list");
    if (group == ServerGroup::Discovery) {
        removeDiscovery(index);
        return;
    }
    NetworkSettings candidate = settings_;
    const ServerEntry &row = rows[index];
    if (row.kind == ServerKind::Stun) {
        QStringList list = listOf(settings_.stun, defaults::stun(), commas());
        refuseEmptying(list, "Keep at least one STUN server.");
        list.removeAll(row.address);
        candidate.stun = stored(list, defaults::stun(), QStringLiteral(", "));
    } else {
        QStringList list = listOf(settings_.turn, defaults::turn(), commas());
        list.removeIf([&](const QString &turn) { return turnAddress(turn) == row.address; });
        candidate.turn = list.isEmpty() ? defaults::noTurn()
                                        : stored(list, defaults::turn(), QStringLiteral(", "));
    }
    commit(candidate);
}

void ServerList::removeDiscovery(std::size_t index) {
    const std::vector<ServerEntry> rows = discoveryEntries();
    const ServerEntry &row = rows[index];
    if (row.kind == ServerKind::Dht && !browser_) {
        QStringList nodes = listOf(settings_.bootstrap, defaults::bootstrap(), commas());
        refuseEmptying(nodes, "Keep at least one DHT node: it is how this app finds others.");
        nodes.removeAll(row.address);
        NetworkSettings candidate = settings_;
        candidate.bootstrap = stored(nodes, defaults::bootstrap(), QStringLiteral(", "));
        commit(candidate);
        return;
    }
    QStringList list = discoveryLines();
    if (row.kind == ServerKind::Dht) {
        QStringList urls = proxies();
        urls.removeAll(row.address);
        list.removeIf([](const QString &line) { return line.startsWith(proxyPrefix); });
        if (!urls.isEmpty())
            list.prepend(proxyPrefix + urls.join(QLatin1Char(' ')));
    } else {
        list.removeAll(kindName(row.kind) + QLatin1Char(' ') + row.address);
    }
    if (list.isEmpty())
        throw std::invalid_argument("Keep at least one server for finding other devices.");
    setDiscoveryLines(list);
}

void ServerList::reset(ServerGroup group) {
    NetworkSettings candidate = settings_;
    if (group == ServerGroup::Discovery) {
        candidate.bootstrap.clear();
        candidate.discovery.clear();
    } else {
        candidate.stun.clear();
        candidate.turn.clear();
    }
    commit(candidate);
}

// Keeps a change only when the configuration it gives would work.
void ServerList::commit(const NetworkSettings &candidate) {
    static_cast<void>(NetworkConfig::from(candidate));
    settings_ = candidate;
}

} // namespace peerchalk::sync
