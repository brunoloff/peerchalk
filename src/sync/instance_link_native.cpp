#include "sync/instance_link.hpp"

#include <QCryptographicHash>
#include <QLocalServer>
#include <QLocalSocket>
#include <QtEndian>
#include <QtLogging>

#include <algorithm>
#include <map>

namespace peerchalk::sync {
namespace {

constexpr quint32 maxFrameBytes = 256U * 1024U * 1024U;
// macOS refuses large writes to a local socket (ENOBUFS) instead of taking part of them, so
// messages go out in pieces, each once the one before has been written.
constexpr qsizetype writeChunkBytes = 64 * 1024;

struct Connection {
    QByteArray incoming;
    QByteArray outgoing;
};

QString serverName(const QString &location, int index) {
    const QByteArray hash =
        QCryptographicHash::hash(location.toUtf8(), QCryptographicHash::Sha256).toHex().left(24);
    return QStringLiteral("peerchalk-%1-%2").arg(QString::fromLatin1(hash)).arg(index);
}

class SocketLink final : public InstanceLink {
  public:
    explicit SocketLink(const InstanceSlot &slot) {
        const QString name = serverName(slot.location(), slot.index());
        QLocalServer::removeServer(name); // left behind if an earlier instance crashed
        server_.setSocketOptions(QLocalServer::UserAccessOption);
        if (!server_.listen(name))
            qWarning("PeerChalk instances cannot connect: %s", qPrintable(server_.errorString()));
        connect(&server_, &QLocalServer::newConnection, this, [this] {
            while (QLocalSocket *socket = server_.nextPendingConnection())
                adopt(socket);
            emit instanceJoined();
        });
        for (const OtherInstance &other : slot.others())
            if (other.running)
                connectTo(serverName(slot.location(), other.index));
    }

    void send(const QByteArray &message) override {
        QByteArray header(4, '\0');
        qToLittleEndian(static_cast<quint32>(message.size()), header.data());
        for (auto &[socket, connection] : sockets_) {
            connection.outgoing += header + message;
            write(socket);
        }
    }

  private:
    void connectTo(const QString &name) {
        auto *socket = new QLocalSocket(this);
        connect(socket, &QLocalSocket::connected, this, [this, socket] {
            adopt(socket);
            emit instanceJoined();
        });
        connect(socket, &QLocalSocket::errorOccurred, socket, [socket] {
            if (socket->state() != QLocalSocket::ConnectedState)
                socket->deleteLater();
        });
        socket->connectToServer(name);
    }

    void adopt(QLocalSocket *socket) {
        socket->setParent(this);
        sockets_[socket] = {};
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { read(socket); });
        connect(socket, &QLocalSocket::bytesWritten, this, [this, socket] { write(socket); });
        connect(socket, &QLocalSocket::errorOccurred, this, [socket] {
            if (socket->error() != QLocalSocket::PeerClosedError)
                qWarning("PeerChalk instance link: %s", qPrintable(socket->errorString()));
        });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            sockets_.erase(socket);
            socket->deleteLater();
        });
        read(socket);
    }

    // Hands the socket the next piece once it has written the last.
    void write(QLocalSocket *socket) {
        const auto found = sockets_.find(socket);
        if (found == sockets_.end() || socket->bytesToWrite() > 0)
            return;
        QByteArray &outgoing = found->second.outgoing;
        const qsizetype size = std::min(outgoing.size(), writeChunkBytes);
        if (size == 0)
            return;
        const qint64 written = socket->write(outgoing.constData(), size);
        if (written > 0)
            outgoing.remove(0, static_cast<qsizetype>(written));
    }

    void read(QLocalSocket *socket) {
        QByteArray &buffer = sockets_.at(socket).incoming;
        buffer += socket->readAll();
        while (buffer.size() >= 4) {
            const auto size = qFromLittleEndian<quint32>(buffer.constData());
            if (size > maxFrameBytes) {
                socket->abort();
                return;
            }
            if (buffer.size() - 4 < static_cast<qsizetype>(size))
                return;
            const QByteArray message = buffer.mid(4, size);
            buffer.remove(0, 4 + static_cast<qsizetype>(size));
            emit received(message);
            if (!sockets_.contains(socket))
                return;
        }
    }

    QLocalServer server_;
    std::map<QLocalSocket *, Connection> sockets_;
};

} // namespace

std::unique_ptr<InstanceLink> InstanceLink::create(const InstanceSlot &slot) {
    return std::make_unique<SocketLink>(slot);
}

} // namespace peerchalk::sync
