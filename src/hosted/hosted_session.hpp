#pragma once

#include "hosted/hosted_bridge.hpp"
#include "hosted/hosted_messages.hpp"
#include "repository/board_writer.hpp"

#include <QObject>
#include <QTimer>
#include <QUuid>

#include <map>
#include <memory>
#include <optional>
#include <set>

namespace peerchalk {
class Library;
namespace sync {
class Network;
}
} // namespace peerchalk

namespace peerchalk::hosted {

// PeerChalk hosted by a dashboard (docs/board-files.md): waits for "open", prepares an
// account for the board, and once the app has signed in to it (attach), sends the dashboard
// this device's change file after edits ("save"), commits through it while this device is
// the session's scribe, and takes in what the dashboard pulls ("update").
class HostedSession : public QObject {
    Q_OBJECT

  public:
    // Timings of the scribe: a save follows edits after saveDelay; the scribe pushes after
    // idleDelay without edits, and at least every pushInterval while edits go on.
    struct Timing {
        int saveDelay{500};
        int idleDelay{5000};
        int pushInterval{30000};
    };
    HostedSession(std::unique_ptr<HostedBridge> bridge, QString accountDirectory, Timing timing,
                  QObject *parent = nullptr);

    void start();
    // The app signed in to the prepared account; library and network outlive the session's
    // use of them until detach.
    void attach(Library &library, sync::Network &network);
    [[nodiscard]] QUuid board() const { return board_; }
    // Where the board's Back button leads (the host's "open" said), or "" for no button.
    [[nodiscard]] QString backLabel() const { return open_ ? open_->backLabel : QString(); }
    // The user pressed Back: the host leaves the board (it sends "close" as it does).
    void requestBack();

  signals:
    // The account in accountDirectory is ready: sign in to it and show the board.
    void opened(const QUuid &board);

  private:
    void poll();
    void receive(const QJsonObject &message);
    void open(const QJsonObject &message);
    void update(const QJsonObject &message);
    void pushed(const QJsonObject &message);
    void close();
    void edited(const QUuid &board);
    void save(bool push);
    void scribeChanged(bool isScribe);
    [[nodiscard]] repository::FileChanges accessFiles() const;
    [[nodiscard]] bool ownEditsPushed() const;
    [[nodiscard]] QJsonArray authors() const;
    void fail(const QString &text);

    std::unique_ptr<HostedBridge> bridge_;
    QString directory_;
    Timing timing_;
    QTimer pollTimer_;
    QTimer saveTimer_;
    QTimer idleTimer_;
    QTimer pushTimer_;
    QUuid board_;
    std::optional<OpenMessage> open_;
    repository::LoadedBoard loaded_;
    std::unique_ptr<UserIdentity> identity_;
    std::set<std::uint64_t> clients_;
    Library *library_{nullptr};
    sync::Network *network_{nullptr};
    int revision_{0};
    std::map<int, QByteArray> savedStates_; // revision -> state vector saved with it
    std::set<QString> authors_;             // names of those present since the last push
    bool unpushed_{false};
    bool canPush_{false};
};

} // namespace peerchalk::hosted
