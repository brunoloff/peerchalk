#include "sync/stun_message.hpp"

#include <QtEndian>

#include <array>
#include <stdexcept>

namespace peerchalk::sync {
namespace {

constexpr quint16 bindingRequest = 0x0001;
constexpr quint16 bindingSuccess = 0x0101;
constexpr quint16 bindingError = 0x0111;
constexpr quint32 magicCookie = 0x2112A442;
constexpr int headerSize = 20;
constexpr int transactionSize = 12;

} // namespace

QByteArray stunBindingRequest(const QByteArray &transactionId) {
    if (transactionId.size() != transactionSize)
        throw std::invalid_argument("A STUN transaction id has 12 bytes");
    std::array<uchar, 8> start{};
    qToBigEndian(bindingRequest, start.data());
    qToBigEndian(quint16{0}, start.data() + 2); // no attributes
    qToBigEndian(magicCookie, start.data() + 4);
    QByteArray message(reinterpret_cast<const char *>(start.data()), start.size());
    return message + transactionId;
}

bool isStunBindingAnswer(const QByteArray &datagram, const QByteArray &transactionId) {
    if (datagram.size() < headerSize || transactionId.size() != transactionSize)
        return false;
    const auto *bytes = reinterpret_cast<const uchar *>(datagram.constData());
    const quint16 type = qFromBigEndian<quint16>(bytes);
    const quint16 length = qFromBigEndian<quint16>(bytes + 2);
    return (type == bindingSuccess || type == bindingError) &&
           qFromBigEndian<quint32>(bytes + 4) == magicCookie &&
           datagram.size() == headerSize + length &&
           datagram.mid(8, transactionSize) == transactionId;
}

} // namespace peerchalk::sync
