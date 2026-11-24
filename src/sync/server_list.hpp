#pragma once

#include "sync/network_settings.hpp"

#include <QString>
#include <QStringList>

#include <cstddef>
#include <vector>

namespace peerchalk::sync {

enum class ServerKind { Dht, Nostr, Mqtt, PeerJs, Stun, Turn };
// Finding other devices, or getting through firewalls to them.
enum class ServerGroup { Discovery, Firewall };

// "dht", "nostr", "mqtt", "peerjs", "stun" or "turn", as the settings page names kinds.
[[nodiscard]] const char *serverKindName(ServerKind kind);
// The kind with that name; throws std::invalid_argument for another.
[[nodiscard]] ServerKind serverKindNamed(const QString &name);

// One server as the settings page lists it.
struct ServerEntry {
    ServerKind kind{ServerKind::Dht};
    QString address;         // what to show and check; a TURN server's login left out
    bool isDefault{false};   // one of PeerChalk's defaults
    bool browserOnly{false}; // used only by the browser version, on desktops
    QString login;           // a TURN server's user:password, for checks; never shown
    friend bool operator==(const ServerEntry &, const ServerEntry &) = default;
};

// The servers network settings name, as lists one can add to, remove from and reset.
// Settings keep their text form; a list equal to the defaults is saved empty, so that new
// defaults reach people who never changed it. On desktops the DHT is reached through
// bootstrap nodes; browsers reach it through OpenDHT proxies, and alone use Nostr, MQTT and
// PeerJS servers (until p2pdc's native nodes do).
class ServerList {
  public:
    ServerList(NetworkSettings settings, bool browser);

    [[nodiscard]] std::vector<ServerEntry> entries(ServerGroup group) const;
    // Adds a server; address as PeerChalk asks for it: host:port for DHT nodes, STUN and
    // TURN (user:password@host:port), an https:// proxy for the DHT in browsers, and a
    // wss:// URL for the others (a PeerJS key after a space). Throws std::invalid_argument
    // with a message for people when the result would not work or the server is listed.
    void add(ServerKind kind, const QString &address);
    // Removes the row at index of entries(group); throws std::out_of_range.
    void remove(ServerGroup group, std::size_t index);
    void reset(ServerGroup group);
    [[nodiscard]] const NetworkSettings &settings() const { return settings_; }

  private:
    [[nodiscard]] QStringList discoveryLines() const;
    void setDiscoveryLines(const QStringList &lines);
    [[nodiscard]] QStringList proxies() const;
    void addDiscovery(ServerKind kind, const QString &address);
    void removeDiscovery(std::size_t index);
    [[nodiscard]] std::vector<ServerEntry> discoveryEntries() const;
    [[nodiscard]] std::vector<ServerEntry> firewallEntries() const;
    void commit(const NetworkSettings &candidate);

    NetworkSettings settings_;
    bool browser_;
};

} // namespace peerchalk::sync
