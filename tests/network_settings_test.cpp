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
    ASSERT_EQ(config.bootstrap.size(), 1U);
    EXPECT_EQ(config.bootstrap.front().host, "bootstrap.jami.net");
    EXPECT_EQ(config.bootstrap.front().port, 4222);
    EXPECT_EQ(config.networkId, 0U);
    ASSERT_EQ(config.ice.servers.size(), 2U); // the public STUN servers
    const auto *stun = std::get_if<p2pdc::StunServer>(&config.ice.servers.front());
    ASSERT_NE(stun, nullptr);
    EXPECT_EQ(stun->host, "stun.cloudflare.com");
    EXPECT_EQ(stun->port, 3478);
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
