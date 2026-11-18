#pragma once

#include "sync/messages.hpp"

#include <QByteArray>

#include <vector>

namespace peerchalk::sync {

// p2pdc limits one message to 2 MiB, so larger ones travel as parts.
inline constexpr qsizetype maxPartBytes = 1024 * 1024;
// The largest message a member of a shared item may send, and anyone else.
inline constexpr qsizetype maxMessageBytes = qsizetype{128} * 1024 * 1024;
inline constexpr qsizetype maxUntrustedMessageBytes = qsizetype{4} * 1024 * 1024;

// An encoded message ready to send: itself if small enough, otherwise encoded parts.
[[nodiscard]] std::vector<QByteArray> splitMessage(const QByteArray &encoded);

// Rebuilds a split message. Connections keep order, so parts must arrive in sequence.
class MessageAssembler {
  public:
    enum class Status { Incomplete, Complete, Invalid };

    // Invalid means the peer broke the rules or exceeded limit; the state is then reset.
    [[nodiscard]] Status add(const MessagePart &part, qsizetype limit);
    // The finished message after add returned Complete.
    [[nodiscard]] QByteArray take();

  private:
    void reset();

    QByteArray bytes_;
    quint32 next_{0};
    quint32 count_{0};
};

} // namespace peerchalk::sync
