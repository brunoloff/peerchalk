#pragma once

#include "sync/messages.hpp"
#include "sync/scribe.hpp"

#include <QDateTime>
#include <QObject>
#include <QUuid>

#include <map>
#include <optional>

namespace peerchalk::sync {

class Network;

// The scribes of the repository boards this device has open (docs/board-files.md): which
// device commits for each live session, and what the session last pushed. It hears peers'
// RepoStatus and RepoPushed through Network, and tells them this device's.
class RepositoryScribe : public QObject {
    Q_OBJECT

  public:
    explicit RepositoryScribe(Network &network);

    // This device has a repository board open; canPush says whether its dashboard can push.
    void setLocal(const QUuid &board, bool canPush);
    void closeLocal(const QUuid &board);
    [[nodiscard]] bool isScribe(const QUuid &board) const;
    // The device that commits for the session on this board, if one can.
    [[nodiscard]] std::optional<QByteArray> scribe(const QUuid &board) const;
    // Tells the session that the branch at commit holds the board up to stateVector.
    void announcePushed(const QUuid &board, const QByteArray &commit,
                        const QByteArray &stateVector);
    [[nodiscard]] std::optional<RepoPushed> lastPushed(const QUuid &board) const;
    // When lastPushed was heard of; null if never.
    [[nodiscard]] QDateTime lastPushedAt(const QUuid &board) const;

    // From Network: a peer's message, and a peer that is ready to hear this device's status.
    void receive(const QByteArray &device, const Message &message);
    [[nodiscard]] std::vector<Message> statusFor(const QUuid &board) const;
    [[nodiscard]] std::vector<QUuid> localBoards() const;
    // Peers came or went: the scribe may have changed.
    void reconsider();

  signals:
    void scribeChanged(const QUuid &board, bool isScribe);
    // A peer pushed this board (or this device did, through announcePushed).
    void pushed(const QUuid &board, const QByteArray &commit, const QByteArray &stateVector);

  private:
    [[nodiscard]] std::vector<ScribeCandidate> candidates(const QUuid &board) const;

    Network &network_;
    std::map<QUuid, bool> local_;                       // board -> canPush
    std::map<QUuid, std::map<QByteArray, bool>> peers_; // board -> device -> canPush
    std::map<QUuid, RepoPushed> pushed_;
    std::map<QUuid, QDateTime> pushedAt_;
    std::map<QUuid, bool> wasScribe_;
};

} // namespace peerchalk::sync
