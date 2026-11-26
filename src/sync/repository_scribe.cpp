#include "sync/repository_scribe.hpp"

#include "sync/network.hpp"
#include "ui/library.hpp"

namespace peerchalk::sync {
namespace {

QByteArray keyBytes(const p2pdc::SigningPublicKey &key) {
    return {reinterpret_cast<const char *>(key.data()), static_cast<qsizetype>(key.size())};
}

} // namespace

RepositoryScribe::RepositoryScribe(Network &network) : QObject(&network), network_(network) {}

std::vector<ScribeCandidate> RepositoryScribe::candidates(const QUuid &board) const {
    const Workspace &workspace = network_.library_.workspace();
    const auto local = local_.find(board);
    std::vector<ScribeCandidate> result{{network_.library_.identityStore().identity().deviceKey(),
                                         workspace.mayAccessBoard(workspace.user(), board),
                                         local != local_.end() && local->second}};
    const auto known = peers_.find(board);
    for (const auto &[id, peer] : network_.peers_) {
        if (!peer->ready || !peer->user || !network_.sharesBoard(*peer, board))
            continue;
        const QByteArray device = keyBytes(peer->device);
        bool canPush = false;
        if (known != peers_.end()) {
            const auto status = known->second.find(device);
            canPush = status != known->second.end() && status->second;
        }
        result.push_back({device, workspace.mayAccessBoard(peer->user->userKey, board), canPush});
    }
    return result;
}

bool RepositoryScribe::isScribe(const QUuid &board) const {
    const std::optional<QByteArray> device = scribe(board);
    return device && *device == network_.library_.identityStore().identity().deviceKey();
}

std::optional<QByteArray> RepositoryScribe::scribe(const QUuid &board) const {
    if (!local_.contains(board))
        return std::nullopt;
    return electScribe(candidates(board));
}

QDateTime RepositoryScribe::lastPushedAt(const QUuid &board) const {
    const auto found = pushedAt_.find(board);
    return found == pushedAt_.end() ? QDateTime() : found->second;
}

void RepositoryScribe::reconsider() {
    for (const auto &[board, canPush] : local_) {
        const bool now = isScribe(board);
        bool &before = wasScribe_[board];
        if (now != before) {
            before = now;
            emit scribeChanged(board, now);
        }
    }
}

void RepositoryScribe::setLocal(const QUuid &board, bool canPush) {
    local_[board] = canPush;
    for (const auto &[id, peer] : network_.peers_) {
        if (peer->ready && peer->user && network_.sharesBoard(*peer, board)) {
            network_.queue(*peer, RepoStatus{board, canPush});
            network_.flush(id);
        }
    }
    reconsider();
}

void RepositoryScribe::closeLocal(const QUuid &board) {
    local_.erase(board);
    wasScribe_.erase(board);
}

void RepositoryScribe::announcePushed(const QUuid &board, const QByteArray &commit,
                                      const QByteArray &stateVector) {
    const RepoPushed message{board, commit, stateVector};
    pushed_[board] = message;
    pushedAt_[board] = QDateTime::currentDateTimeUtc();
    for (const auto &[id, peer] : network_.peers_) {
        if (peer->ready && peer->user && network_.sharesBoard(*peer, board)) {
            network_.queue(*peer, message);
            network_.flush(id);
        }
    }
    emit pushed(board, commit, stateVector);
}

std::optional<RepoPushed> RepositoryScribe::lastPushed(const QUuid &board) const {
    const auto found = pushed_.find(board);
    return found == pushed_.end() ? std::nullopt : std::optional(found->second);
}

void RepositoryScribe::receive(const QByteArray &device, const Message &message) {
    if (const auto *status = std::get_if<RepoStatus>(&message)) {
        peers_[status->board][device] = status->canPush;
        reconsider();
    } else if (const auto *pushed = std::get_if<RepoPushed>(&message);
               pushed && local_.contains(pushed->board)) {
        pushed_[pushed->board] = *pushed;
        pushedAt_[pushed->board] = QDateTime::currentDateTimeUtc();
        emit this->pushed(pushed->board, pushed->commit, pushed->stateVector);
    }
}

std::vector<Message> RepositoryScribe::statusFor(const QUuid &board) const {
    const auto local = local_.find(board);
    if (local == local_.end())
        return {};
    std::vector<Message> messages{RepoStatus{board, local->second}};
    if (const std::optional<RepoPushed> pushed = lastPushed(board))
        messages.emplace_back(*pushed);
    return messages;
}

std::vector<QUuid> RepositoryScribe::localBoards() const {
    std::vector<QUuid> boards;
    for (const auto &[board, canPush] : local_)
        boards.push_back(board);
    return boards;
}

} // namespace peerchalk::sync
