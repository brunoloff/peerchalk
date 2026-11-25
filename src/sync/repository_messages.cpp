// RepoStatus and RepoPushed: a type byte, then a board id, then the fields.
#include "sync/messages.hpp"

namespace peerchalk::sync {
namespace {

constexpr char repoStatusType = 14;
constexpr char repoPushedType = 15;
constexpr qsizetype uuidSize = 16;
constexpr qsizetype maxStateVectorBytes = 64 * 1024;

QByteArray header(char type, const QUuid &board) { return QByteArray(1, type) + board.toRfc4122(); }

} // namespace

QByteArray encodeRepoStatus(const RepoStatus &status) {
    return header(repoStatusType, status.board) + QByteArray(1, status.canPush ? '\1' : '\0');
}

QByteArray encodeRepoPushed(const RepoPushed &pushed) {
    return header(repoPushedType, pushed.board) +
           QByteArray(1, static_cast<char>(pushed.commit.size())) + pushed.commit +
           pushed.stateVector;
}

bool isRepoMessage(char type) { return type == repoStatusType || type == repoPushedType; }

std::optional<Message> decodeRepoMessage(const QByteArray &bytes) {
    if (bytes.size() < 2 + uuidSize || !isRepoMessage(bytes.at(0)))
        return std::nullopt;
    const QUuid board = QUuid::fromRfc4122(bytes.mid(1, uuidSize));
    const QByteArray rest = bytes.mid(1 + uuidSize);
    if (board.isNull())
        return std::nullopt;
    if (bytes.at(0) == repoStatusType) {
        if (rest.size() != 1 || static_cast<unsigned char>(rest.at(0)) > 1)
            return std::nullopt;
        return RepoStatus{board, rest.at(0) == '\1'};
    }
    const auto commitSize = static_cast<qsizetype>(static_cast<unsigned char>(rest.at(0)));
    if ((commitSize != 20 && commitSize != maxCommitBytes) || rest.size() < 1 + commitSize ||
        rest.size() - 1 - commitSize > maxStateVectorBytes)
        return std::nullopt;
    return RepoPushed{board, rest.mid(1, commitSize), rest.mid(1 + commitSize)};
}

} // namespace peerchalk::sync
