#pragma once

#include "sync/server_list.hpp"

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

namespace peerchalk::sync {

// A server to check, as listed on the settings page; login is a TURN server's
// user:password, which the check needs but the page never shows.
struct ServerTarget {
    QString key; // how results name it: kind and address
    ServerKind kind{ServerKind::Dht};
    QString address;
    QString login;
};

// Checks whether servers answer: a TLS handshake (desktops) or a WebSocket or HTTP request
// (browsers) for web servers, a STUN Binding request for STUN and TURN servers (in
// browsers, a WebRTC candidate of the right kind), and a name lookup for DHT nodes, whose
// real test is the DHT going online. Shows reachability, not that a server serves PeerChalk.
class ServerChecks : public QObject {
    Q_OBJECT

  public:
    explicit ServerChecks(QObject *parent = nullptr);
    ~ServerChecks() override;
    ServerChecks(const ServerChecks &) = delete;
    ServerChecks &operator=(const ServerChecks &) = delete;

    // Starts checking each target, abandoning checks still running; each answers once.
    void check(const std::vector<ServerTarget> &targets);
#ifdef __EMSCRIPTEN__
    // web_checks.js's answer to the check with that id.
    void answered(int id, bool working, int milliseconds);
#endif

  signals:
    void checked(const QString &key, bool working, int milliseconds);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace peerchalk::sync
