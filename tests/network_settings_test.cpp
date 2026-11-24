#include "sync/network_config.hpp"
#include "sync/network_settings.hpp"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <stdexcept>
#include <variant>

namespace peerchalk::sync {
namespace {

// Runs each test without the PEERCHALK_* network variables.
class NetworkSettingsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        for (const char *name : {"PEERCHALK_NETWORK", "PEERCHALK_BOOTSTRAP",
                                 "PEERCHALK_DHT_NETWORK", "PEERCHALK_STUN", "PEERCHALK_TURN"})
            qunsetenv(name);
    }
    void TearDown() override { SetUp(); }
    QTemporaryDir dir_;
};

TEST_F(NetworkSettingsTest, DefaultsToThePublicNetwork) {
    const NetworkSettings settings = NetworkSettings::load(dir_.path());
    EXPECT_TRUE(settings.enabled);
    const NetworkConfig config = NetworkConfig::from(settings);
    ASSERT_EQ(config.bootstrap.size(), 2U); // Jami's node, then PeerChalk's
    EXPECT_EQ(config.bootstrap.front().host, "bootstrap.jami.net");
    EXPECT_EQ(config.bootstrap.front().port, 4222);
    EXPECT_EQ(config.bootstrap.back().host, "89.168.35.4");
    EXPECT_EQ(config.networkId, 0U);
    ASSERT_EQ(config.ice.servers.size(), 3U); // the public STUN servers and PeerChalk's relay
    const auto *stun = std::get_if<p2pdc::StunServer>(&config.ice.servers.front());
    ASSERT_NE(stun, nullptr);
    EXPECT_EQ(stun->host, "stun.cloudflare.com");
    EXPECT_EQ(stun->port, 3478);
    const auto *turn = std::get_if<p2pdc::TurnServer>(&config.ice.servers.back());
    ASSERT_NE(turn, nullptr);
    EXPECT_EQ(turn->host, "89.168.35.4");
    EXPECT_EQ(turn->port, 3478);
    EXPECT_EQ(turn->username, "peerchalk");
    EXPECT_EQ(config.browserBackends,
              (std::vector<p2pdc::BrowserBackend>{
                  p2pdc::OpenDhtProxies{{"https://dhtproxy.jami.net", "https://89-168-35-4.sslip.io"}},
                  p2pdc::NostrRelay{"wss://nos.lol"}, p2pdc::NostrRelay{"wss://relay.primal.net"}}));
}

TEST_F(NetworkSettingsTest, NoneTurnsTheRelayOff) {
    NetworkSettings settings;
    settings.turn = QStringLiteral(" None ");
    const NetworkConfig config = NetworkConfig::from(settings);
    ASSERT_EQ(config.ice.servers.size(), 2U); // only the public STUN servers
    for (const auto &server : config.ice.servers)
        EXPECT_EQ(std::get_if<p2pdc::TurnServer>(&server), nullptr);
    settings.save(dir_.path());
    EXPECT_EQ(NetworkSettings::load(dir_.path()).turn, QStringLiteral("None"));
}

TEST_F(NetworkSettingsTest, DiscoveryServersAreReadOnePerLine) {
    NetworkSettings settings;
    settings.discovery = QStringLiteral(
        "  opendht https://one.example https://two.example/base\n\n# a comment\nmqtt wss://broker.example/mqtt ; "
        "peerjs ws://127.0.0.1:9000/peerjs key\n");
    EXPECT_EQ(NetworkConfig::from(settings).browserBackends,
              (std::vector<p2pdc::BrowserBackend>{
                  p2pdc::OpenDhtProxies{{"https://one.example", "https://two.example/base"}},
                  p2pdc::MqttBroker{"wss://broker.example/mqtt"},
                  p2pdc::PeerJsServer{"ws://127.0.0.1:9000/peerjs", "key"}}));
    settings.save(dir_.path());
    EXPECT_EQ(NetworkSettings::load(dir_.path()).discovery, settings.discovery.trimmed());
}

TEST_F(NetworkSettingsTest, DiscoveryServersMustBeOnesABrowserCanUse) {
    NetworkSettings settings;
    for (const char *rejected : {"https://proxy.example", "nostr https://relay.example",
                                 "opendht http://proxy.example", "mqtt", "# only a comment",
                                 "smoke wss://signals.example"}) {
        settings.discovery = QString::fromLatin1(rejected);
        EXPECT_THROW(static_cast<void>(NetworkConfig::from(settings)), std::invalid_argument) << rejected;
    }
    QStringList nine;
    for (int i = 0; i < 9; ++i)
        nine << QStringLiteral("nostr wss://r%1.example").arg(i);
    settings.discovery = nine.join(QLatin1Char('\n'));
    EXPECT_THROW(static_cast<void>(NetworkConfig::from(settings)), std::invalid_argument);
}

TEST_F(NetworkSettingsTest, SavedSettingsAreLoadedAgain) {
    NetworkSettings settings;
    settings.enabled = false;
    settings.bootstrap = QStringLiteral("a.example:1, b.example:2");
    settings.dhtNetwork = QStringLiteral("7");
    settings.stun = QStringLiteral("stun.example:3478");
    settings.turn = QStringLiteral("me:secret@turn.example:3478");
    settings.save(dir_.path());
    const NetworkSettings loaded = NetworkSettings::load(dir_.path());
    EXPECT_EQ(loaded.bootstrap, QStringLiteral("a.example:1, b.example:2"));
    EXPECT_FALSE(loaded.enabled);
    const NetworkConfig config = NetworkConfig::from(loaded);
    EXPECT_EQ(config.bootstrap.size(), 2U);
    EXPECT_EQ(config.networkId, 7U);
    ASSERT_EQ(config.ice.servers.size(), 2U);
    const auto *turn = std::get_if<p2pdc::TurnServer>(&config.ice.servers[1]);
    ASSERT_NE(turn, nullptr);
    EXPECT_EQ(turn->username, "me");
    EXPECT_EQ(turn->password, "secret");
}

TEST_F(NetworkSettingsTest, MalformedSettingsAreNotSaved) {
    NetworkSettings settings;
    settings.turn = QStringLiteral("turn.example:3478");
    EXPECT_THROW(settings.save(dir_.path()), std::invalid_argument);
    settings.turn.clear();
    settings.dhtNetwork = QStringLiteral("seven");
    EXPECT_THROW(settings.save(dir_.path()), std::invalid_argument);
    settings.dhtNetwork.clear();
    settings.stun = QStringLiteral("stun.example");
    EXPECT_THROW(settings.save(dir_.path()), std::invalid_argument);
    EXPECT_TRUE(NetworkSettings::load(dir_.path()).stun.isEmpty()); // nothing was written
}

TEST_F(NetworkSettingsTest, EnvironmentVariablesTakePrecedence) {
    NetworkSettings settings;
    settings.stun = QStringLiteral("saved.example:1");
    EXPECT_FALSE(NetworkSettings::environmentOverrides());
    EXPECT_EQ(settings.withEnvironment(), settings);
    qputenv("PEERCHALK_STUN", "env.example:2");
    qputenv("PEERCHALK_NETWORK", "off");
    const NetworkSettings effective = settings.withEnvironment();
    EXPECT_TRUE(NetworkSettings::environmentOverrides());
    EXPECT_EQ(effective.stun, QStringLiteral("env.example:2"));
    EXPECT_FALSE(effective.enabled);
}

} // namespace
} // namespace peerchalk::sync
