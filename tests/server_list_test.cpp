#include "sync/network_config.hpp"
#include "sync/server_list.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <variant>

namespace peerchalk::sync {
namespace {

int turnServers(const NetworkSettings &settings) {
    int count = 0;
    for (const auto &server : NetworkConfig::from(settings).ice.servers)
        count += std::holds_alternative<p2pdc::TurnServer>(server) ? 1 : 0;
    return count;
}

TEST(ServerListTest, ListsTheDefaultsForDesktopsAndBrowsers) {
    const ServerList desktop({}, false);
    const auto found = desktop.entries(ServerGroup::Discovery);
    ASSERT_EQ(found.size(), 6U); // two DHT nodes, then two Nostr relays, MQTT and PeerJS for browsers
    EXPECT_EQ(found[0], (ServerEntry{ServerKind::Dht, "bootstrap.jami.net:4222", true, false, {}}));
    EXPECT_EQ(found[2], (ServerEntry{ServerKind::Nostr, "wss://nos.lol", true, true, {}}));
    EXPECT_EQ(found[4], (ServerEntry{ServerKind::Mqtt, "wss://broker.hivemq.com:8884/mqtt", true, true, {}}));
    EXPECT_EQ(found[5], (ServerEntry{ServerKind::PeerJs, "wss://0.peerjs.com/peerjs peerjs", true, true, {}}));
    const ServerList browser({}, true);
    EXPECT_EQ(browser.entries(ServerGroup::Discovery)[1],
              (ServerEntry{ServerKind::Dht, "https://89-168-35-4.sslip.io", true, false, {}}));
    const auto firewall = desktop.entries(ServerGroup::Firewall);
    ASSERT_EQ(firewall.size(), 3U);
    EXPECT_EQ(firewall[2], (ServerEntry{ServerKind::Turn, "89.168.35.4:3478", true, false,
                                        "peerchalk:peerchalk-open-relay"}))
        << "a TURN login is kept apart from the address";
}

TEST(ServerListTest, AddsAndRemovesServersAndSavesDefaultsAsEmpty) {
    ServerList list({}, true);
    list.add(ServerKind::Mqtt, " wss://broker.example:8884/mqtt ");
    list.add(ServerKind::Dht, "https://proxy.example");
    EXPECT_EQ(
        list.settings().discovery,
        "opendht https://dhtproxy.jami.net https://89-168-35-4.sslip.io https://proxy.example\n"
        "nostr wss://nos.lol\nnostr wss://relay.primal.net\nmqtt wss://broker.hivemq.com:8884/mqtt\n"
        "peerjs wss://0.peerjs.com/peerjs peerjs\nmqtt wss://broker.example:8884/mqtt");
    const auto rows = list.entries(ServerGroup::Discovery);
    EXPECT_FALSE(rows.back().isDefault);
    list.remove(ServerGroup::Discovery, rows.size() - 1);
    list.remove(ServerGroup::Discovery, 2);
    EXPECT_TRUE(list.settings().discovery.isEmpty()) << "back to the defaults";
    list.add(ServerKind::Stun, "stun.example:3478");
    list.reset(ServerGroup::Firewall);
    EXPECT_TRUE(list.settings().stun.isEmpty());
}

TEST(ServerListTest, TurnServersCanBeAddedAndAllRemoved) {
    ServerList list({}, false);
    list.add(ServerKind::Turn, "me:secret@turn.example:3478");
    EXPECT_EQ(turnServers(list.settings()), 2);
    list.remove(ServerGroup::Firewall, 3);
    list.remove(ServerGroup::Firewall, 2);
    EXPECT_EQ(list.settings().turn, "none");
    EXPECT_EQ(turnServers(list.settings()), 0);
    list.add(ServerKind::Turn, "me:secret@turn.example:3478");
    EXPECT_EQ(list.settings().turn, "me:secret@turn.example:3478");
}

TEST(ServerListTest, RefusesWhatWouldNotWork) {
    ServerList list({}, false);
    EXPECT_THROW(list.add(ServerKind::Nostr, "https://not-a-relay.example"), std::invalid_argument);
    EXPECT_THROW(list.add(ServerKind::Dht, "no-port"), std::invalid_argument);
    EXPECT_THROW(list.add(ServerKind::Nostr, "wss://nos.lol"), std::invalid_argument);
    EXPECT_THROW(list.add(ServerKind::Stun, " "), std::invalid_argument);
    EXPECT_THROW(list.remove(ServerGroup::Firewall, 9), std::out_of_range);
    list.remove(ServerGroup::Discovery, 0);
    EXPECT_THROW(list.remove(ServerGroup::Discovery, 0), std::invalid_argument)
        << "the last DHT node stays";
    EXPECT_EQ(list.entries(ServerGroup::Discovery).size(), 5U);
}

} // namespace
} // namespace peerchalk::sync
