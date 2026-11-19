#include "sync/connection_log.hpp"
#include "sync/p2pdc_names.hpp"

#include <QtLogging>

#include <gtest/gtest.h>

#include <stdexcept>

namespace peerchalk::sync {
namespace {

class ConnectionLogTest : public ::testing::Test {
  protected:
    void SetUp() override { ConnectionLog::instance().clear(); }
};

TEST_F(ConnectionLogTest, FormatsLinesWithTimeLevelAndArea) {
    logWarning(QStringLiteral("connection"), QStringLiteral("timed out"));
    const QStringList lines = ConnectionLog::instance().lines();
    ASSERT_EQ(lines.size(), 1);
    EXPECT_TRUE(lines.front().endsWith(QStringLiteral("Z WARN  connection: timed out")))
        << lines.front().toStdString();
}

TEST_F(ConnectionLogTest, KeepsOnlyTheMostRecentEntries) {
    const quint64 before = ConnectionLog::instance().revision();
    for (std::size_t i = 0; i < ConnectionLog::maxEntries + 10; ++i)
        logInfo(QStringLiteral("test"), QString::number(i));
    const QStringList lines = ConnectionLog::instance().lines();
    EXPECT_EQ(std::size_t(lines.size()), ConnectionLog::maxEntries);
    EXPECT_TRUE(lines.front().endsWith(QStringLiteral(": 10")));
    EXPECT_GT(ConnectionLog::instance().revision(), before);
}

TEST_F(ConnectionLogTest, ShortensLongMessages) {
    logInfo(QStringLiteral("test"), QString(5000, QLatin1Char('x')));
    EXPECT_LT(ConnectionLog::instance().lines().front().size(), 700);
}

TEST_F(ConnectionLogTest, CapturesWarningsButNotOrdinaryDebugOutput) {
    ConnectionLog::captureQtMessages();
    qWarning("something went wrong");
    qDebug("chatter");
    qInfo("PeerChalk: own message");
    const QString all = ConnectionLog::instance().lines().join(QLatin1Char('\n'));
    EXPECT_TRUE(all.contains(QStringLiteral("WARN  app: something went wrong")));
    EXPECT_FALSE(all.contains(QStringLiteral("chatter")));
    EXPECT_TRUE(all.contains(QStringLiteral("PeerChalk: own message")));
}

TEST(P2pdcNames, DescribeResultsAndKeys) {
    EXPECT_EQ(describe(p2pdc::ConnectError::Transport),
              QStringLiteral("transport failed (no network path)"));
    EXPECT_EQ(describe(p2pdc::AnnouncementError::Retrying), QStringLiteral("retrying"));
    p2pdc::RouteInfo route;
    route.current = p2pdc::RouteKind::TurnRelayed;
    route.selected_pair = p2pdc::CandidatePair{p2pdc::CandidateKind::Host,
                                               p2pdc::CandidateKind::Relayed};
    EXPECT_EQ(describe(route), QStringLiteral("TURN relay (host to relayed)"));
    EXPECT_EQ(describe(std::make_exception_ptr(std::runtime_error("no route"))),
              QStringLiteral("no route"));
    EXPECT_TRUE(describe(std::exception_ptr{}).isEmpty());
    p2pdc::SigningPublicKey key{};
    key[0] = std::byte{0xab};
    EXPECT_EQ(shortDevice(key), QStringLiteral("ab000000"));
}

} // namespace
} // namespace peerchalk::sync
