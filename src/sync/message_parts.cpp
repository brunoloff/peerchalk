#include "sync/message_parts.hpp"

#include <utility>

namespace peerchalk::sync {

std::vector<QByteArray> splitMessage(const QByteArray &encoded) {
    if (encoded.size() <= maxPartBytes)
        return {encoded};
    const auto count = static_cast<quint32>((encoded.size() + maxPartBytes - 1) / maxPartBytes);
    std::vector<QByteArray> parts;
    parts.reserve(count);
    for (quint32 index = 0; index < count; ++index)
        parts.push_back(encode(
            MessagePart{index, count,
                        encoded.mid(static_cast<qsizetype>(index) * maxPartBytes, maxPartBytes)}));
    return parts;
}

void MessageAssembler::reset() {
    bytes_.clear();
    next_ = 0;
    count_ = 0;
}

MessageAssembler::Status MessageAssembler::add(const MessagePart &part, qsizetype limit) {
    const bool starts = next_ == 0 && part.index == 0;
    const bool continues = next_ != 0 && part.index == next_ && part.count == count_;
    const bool lastPart = part.index + 1 == part.count;
    const bool sizedRightly = part.data.size() == maxPartBytes ||
                              (lastPart && part.data.size() > 0 && part.data.size() < maxPartBytes);
    if ((!starts && !continues) || !sizedRightly || part.data.size() > limit - bytes_.size()) {
        reset();
        return Status::Invalid;
    }
    count_ = part.count;
    bytes_ += part.data;
    ++next_;
    return lastPart ? Status::Complete : Status::Incomplete;
}

QByteArray MessageAssembler::take() {
    QByteArray result = std::exchange(bytes_, {});
    reset();
    return result;
}

} // namespace peerchalk::sync
