#include "sync/server_checks.hpp"
#include "sync/stun_message.hpp"

#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QUdpSocket>

#include <gtest/gtest.h>

#include <map>

namespace peerchalk::sync {
namespace {

// Answers every STUN Binding request on 127.0.0.1 with a success response.
class FakeStunServer : public QObject {
  public:
    FakeStunServer() {
        socket_.bind(QHostAddress::LocalHost, 0);
        connect(&socket_, &QUdpSocket::readyRead, this, [this] {
            while (socket_.hasPendingDatagrams()) {
                const QNetworkDatagram request = socket_.receiveDatagram();
                QByteArray answer = request.data().left(20);
                answer[0] = 0x01;
                answer[1] = 0x01;
                socket_.writeDatagram(answer, request.senderAddress(), request.senderPort());
            }
        });
    }
    [[nodiscard]] quint16 port() const { return socket_.localPort(); }

  private:
    QUdpSocket socket_;
};

std::map<QString, bool> results(ServerChecks &checks, const std::vector<ServerTarget> &targets) {
    QSignalSpy spy(&checks, &ServerChecks::checked);
    checks.check(targets);
    EXPECT_TRUE(QTest::qWaitFor(
        [&] { return spy.size() == static_cast<qsizetype>(targets.size()); }, 10000));
    std::map<QString, bool> found;
    for (const QList<QVariant> &answer : spy)
        found[answer.at(0).toString()] = answer.at(1).toBool();
    return found;
}

TEST(ServerChecksTest, AStunServerThatAnswersWorksAndASilentOneDoesNot) {
    FakeStunServer server;
    QUdpSocket silent;
    silent.bind(QHostAddress::LocalHost, 0);
    const QString answering = QStringLiteral("127.0.0.1:%1").arg(server.port());
    const QString quiet = QStringLiteral("127.0.0.1:%1").arg(silent.localPort());
    ServerChecks checks;
    const auto found = results(checks, {{"stun", ServerKind::Stun, answering, {}},
                                        {"turn", ServerKind::Turn, quiet, "user:password"}});
    EXPECT_TRUE(found.at("stun"));
    EXPECT_FALSE(found.at("turn"));
}

TEST(ServerChecksTest, DhtNodesAreLookedUp) {
    ServerChecks checks;
    const auto found = results(checks, {{"here", ServerKind::Dht, "localhost:4222", {}},
                                        {"nowhere", ServerKind::Dht, "node.invalid:4222", {}}});
    EXPECT_TRUE(found.at("here"));
    EXPECT_FALSE(found.at("nowhere"));
}

TEST(ServerChecksTest, WebSocketServersGetATlsHandshakeAtTheirHostAndPort) {
    QTcpServer plain; // accepts, then hangs up: no TLS, so the handshake fails
    ASSERT_TRUE(plain.listen(QHostAddress::LocalHost));
    int accepted = 0;
    QObject::connect(&plain, &QTcpServer::newConnection, [&] {
        while (QTcpSocket *socket = plain.nextPendingConnection()) {
            ++accepted;
            socket->disconnectFromHost();
        }
    });
    const QString base = QStringLiteral("wss://127.0.0.1:%1").arg(plain.serverPort());
    ServerChecks checks;
    const auto found =
        results(checks, {{"nostr", ServerKind::Nostr, base, {}},
                         {"mqtt", ServerKind::Mqtt, base + "/mqtt", {}},
                         {"peerjs", ServerKind::PeerJs, base + "/peerjs peerjs", {}}});
    EXPECT_EQ(accepted, 3);
    for (const char *kind : {"nostr", "mqtt", "peerjs"})
        EXPECT_FALSE(found.at(kind)) << kind;
}

TEST(ServerChecksTest, AnotherCheckAbandonsTheFirst) {
    QUdpSocket silent;
    silent.bind(QHostAddress::LocalHost, 0);
    ServerChecks checks;
    QSignalSpy spy(&checks, &ServerChecks::checked);
    checks.check(
        {{"old", ServerKind::Stun, QStringLiteral("127.0.0.1:%1").arg(silent.localPort()), {}}});
    const auto found = results(checks, {{"new", ServerKind::Dht, "localhost:4222", {}}});
    QTest::qWait(4500); // past the first check's timeout
    EXPECT_EQ(found.size(), 1U);
    for (const QList<QVariant> &answer : spy)
        EXPECT_NE(answer.at(0).toString(), "old");
}

} // namespace
} // namespace peerchalk::sync
