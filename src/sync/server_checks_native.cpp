// Server checks on desktops and phones, with Qt's sockets.
#include "sync/server_checks.hpp"

#include "sync/stun_message.hpp"

#include <QElapsedTimer>
#include <QHostInfo>
#include <QNetworkDatagram>
#include <QPointer>
#include <QRandomGenerator>
#include <QSslSocket>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>

#include <functional>

namespace peerchalk::sync {
namespace {

constexpr int tlsTimeoutMs = 8000;
constexpr int stunTimeoutMs = 4000;
constexpr int stunResendMs = 800;

struct Endpoint {
    QString host;
    quint16 port{0};
};

// A URL's host and port, or host:port.
Endpoint endpointOf(const QString &address) {
    const QString first = address.section(QLatin1Char(' '), 0, 0);
    if (first.contains(QStringLiteral("://"))) {
        const QUrl url(first);
        return {url.host(), static_cast<quint16>(url.port(443))};
    }
    const qsizetype colon = first.lastIndexOf(QLatin1Char(':'));
    return {first.left(colon), static_cast<quint16>(first.mid(colon + 1).toUInt())};
}

// One check: reports once, then lets its sockets go with it.
class Probe : public QObject {
  public:
    using Report = std::function<void(bool working, int milliseconds)>;
    Probe(QObject *batch, Report report) : QObject(batch), report_(std::move(report)) {
        clock_.start();
    }
    void finish(bool working) {
        if (done_)
            return;
        done_ = true;
        report_(working, static_cast<int>(clock_.elapsed()));
        deleteLater();
    }

  private:
    Report report_;
    QElapsedTimer clock_;
    bool done_{false};
};

void checkTls(Probe *probe, const Endpoint &server) {
    auto *socket = new QSslSocket(probe);
    QObject::connect(socket, &QSslSocket::encrypted, probe, [probe] { probe->finish(true); });
    QObject::connect(socket, &QAbstractSocket::errorOccurred, probe,
                     [probe] { probe->finish(false); });
    QTimer::singleShot(tlsTimeoutMs, probe, [probe] { probe->finish(false); });
    socket->connectToHostEncrypted(server.host, server.port);
}

void sendStun(Probe *probe, QUdpSocket *socket, const QHostAddress &to, quint16 port,
              const QByteArray &transaction) {
    const QByteArray request = stunBindingRequest(transaction);
    auto *resend = new QTimer(probe);
    QObject::connect(resend, &QTimer::timeout, socket,
                     [=] { socket->writeDatagram(request, to, port); });
    resend->start(stunResendMs);
    socket->writeDatagram(request, to, port);
}

void checkStun(Probe *probe, const Endpoint &server) {
    auto *socket = new QUdpSocket(probe);
    QByteArray transaction(12, '\0');
    for (char &byte : transaction)
        byte = static_cast<char>(QRandomGenerator::global()->bounded(256));
    QObject::connect(socket, &QUdpSocket::readyRead, probe, [=] {
        while (socket->hasPendingDatagrams()) {
            if (isStunBindingAnswer(socket->receiveDatagram().data(), transaction))
                probe->finish(true);
        }
    });
    QTimer::singleShot(stunTimeoutMs, probe, [probe] { probe->finish(false); });
    QPointer<Probe> alive(probe);
    QHostInfo::lookupHost(server.host, probe, [=](const QHostInfo &info) {
        if (!alive || info.error() != QHostInfo::NoError || info.addresses().isEmpty())
            return alive ? alive->finish(false) : void();
        sendStun(probe, socket, info.addresses().constFirst(), server.port, transaction);
    });
}

void checkLookup(Probe *probe, const Endpoint &server) {
    QHostInfo::lookupHost(server.host, probe, [probe](const QHostInfo &info) {
        probe->finish(info.error() == QHostInfo::NoError && !info.addresses().isEmpty());
    });
}

} // namespace

struct ServerChecks::Impl {
    std::unique_ptr<QObject> batch;
};

ServerChecks::ServerChecks(QObject *parent) : QObject(parent), impl_(std::make_unique<Impl>()) {}

ServerChecks::~ServerChecks() = default;

void ServerChecks::check(const std::vector<ServerTarget> &targets) {
    impl_->batch = std::make_unique<QObject>();
    for (const ServerTarget &target : targets) {
        const QString key = target.key;
        auto *probe = new Probe(impl_->batch.get(), [this, key](bool working, int milliseconds) {
            emit checked(key, working, milliseconds);
        });
        const Endpoint server = endpointOf(target.address);
        if (target.kind == ServerKind::Stun || target.kind == ServerKind::Turn)
            checkStun(probe, server);
        else if (target.kind == ServerKind::Dht && !target.address.contains(QStringLiteral("://")))
            checkLookup(probe, server);
        else
            checkTls(probe, server);
    }
}

} // namespace peerchalk::sync
