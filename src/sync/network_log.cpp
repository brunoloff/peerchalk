// Network: the troubleshooting log shown in Settings → Troubleshooting.
#include "library/files.hpp"
#include "sync/connection_log.hpp"
#include "sync/network.hpp"
#include "sync/p2pdc_names.hpp"
#include "ui/library.hpp"
#include "ui/web_files.hpp"

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QSysInfo>

#include <exception>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace peerchalk::sync {

using namespace p2pdc;

namespace {

// Server addresses, without passwords.
QStringList iceServers(const IceConfig &ice) {
    QStringList servers;
    for (const IceServer &server : ice.servers) {
        if (const auto *stun = std::get_if<StunServer>(&server))
            servers << QStringLiteral("STUN %1:%2").arg(QString::fromStdString(stun->host)).arg(stun->port);
        else if (const auto *turn = std::get_if<TurnServer>(&server))
            servers << QStringLiteral("TURN %1:%2").arg(QString::fromStdString(turn->host)).arg(turn->port);
    }
    return servers;
}

QString joined(const std::vector<p2pdc::BrowserBackend> &backends) {
    QStringList list;
    for (const auto &backend : backends)
        list << QString::fromStdString(p2pdc::to_string(backend));
    return list.isEmpty() ? QStringLiteral("none") : list.join(QStringLiteral("; "));
}

QString platform() {
#ifdef __EMSCRIPTEN__
    return QStringLiteral("browser (%1)")
        .arg(QString::fromUtf8(emscripten_run_script_string("navigator.userAgent")));
#else
    return QStringLiteral("%1, %2").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
#endif
}

} // namespace

QString Network::spaceLabel(const QString &key) {
    if (key.startsWith(QLatin1String("p:")))
        return QStringLiteral("project %1").arg(key.mid(2, 8));
    if (key.startsWith(QLatin1String("b:")))
        return QStringLiteral("board %1").arg(key.mid(2, 8));
    return key == QLatin1String("personal") ? QStringLiteral("your devices") : key;
}

void Network::logConfiguration() const {
    QStringList bootstrap;
    for (const BootstrapServer &server : config_.bootstrap)
        bootstrap << QStringLiteral("%1:%2").arg(QString::fromStdString(server.host)).arg(server.port);
    logInfo(QStringLiteral("network"),
            QStringLiteral("Starting PeerChalk %1 on %2 with Qt %3")
                .arg(QStringLiteral(PEERCHALK_VERSION), platform(), QString::fromLatin1(qVersion())));
    logInfo(QStringLiteral("network"),
            QStringLiteral("DHT network %1, bootstrap %2, discovery %3; %4")
                .arg(config_.networkId)
                .arg(bootstrap.join(QStringLiteral(", ")), joined(config_.browserBackends),
                     iceServers(config_.ice).join(QStringLiteral(", "))));
}

QString Network::summary() const {
    QStringList lines{
        QStringLiteral("PeerChalk %1 on %2, Qt %3").arg(QStringLiteral(PEERCHALK_VERSION), platform(), QString::fromLatin1(qVersion())),
        QStringLiteral("Time: %1 (UTC offset %2 min)")
            .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate))
            .arg(QDateTime::currentDateTime().offsetFromUtc() / 60),
        QStringLiteral("Status: %1").arg(status_),
        QStringLiteral("This device: %1").arg(node_ && node_->identity() ? shortDevice(node_->identity()->signing_key) : QStringLiteral("not started")),
        QStringLiteral("Servers: %1").arg(iceServers(config_.ice).join(QStringLiteral(", ")))};
    for (const auto &[key, space] : spaces_) {
        lines << QStringLiteral("Space %1: %2, %3, %4 known devices")
                     .arg(spaceLabel(key), space->published ? QStringLiteral("announced") : QStringLiteral("not announced"),
                          space->caught ? QStringLiteral("lookup done") : QStringLiteral("lookup pending"))
                     .arg(space->advertisements.size());
    }
    for (const auto &[id, peer] : peers_) {
        lines << QStringLiteral("Peer %1: %2, %3 for %4 s%5")
                     .arg(peerLabel(id), peer->ready ? QStringLiteral("ready") : QStringLiteral("not ready"),
                          peer->user ? QStringLiteral("signed in as %1").arg(peer->user->name) : QStringLiteral("not signed in"))
                     .arg(peer->started.elapsed() / 1000)
                     .arg(peer->route.isEmpty() ? QString() : QStringLiteral(", ") + peer->route);
    }
    return lines.join(QLatin1Char('\n'));
}

QString Network::logText(int lastLines) const {
    QStringList lines = ConnectionLog::instance().lines();
    if (lastLines > 0 && lines.size() > lastLines)
        lines = lines.mid(lines.size() - lastLines);
    return summary() + QStringLiteral("\n\n") + lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

void Network::clearLog() { ConnectionLog::instance().clear(); }

void Network::copyLog() const {
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(logText());
}

QString Network::logFileName() {
    return QStringLiteral("peerchalk-log-%1.txt")
        .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-hhmmss")));
}

QString Network::saveLog(const QUrl &file) const {
    try {
        writeFile(file.toLocalFile(), logText().toUtf8(), false);
        return {};
    } catch (const std::exception &error) {
        return QString::fromUtf8(error.what());
    }
}

QString Network::downloadLog() const {
#ifdef __EMSCRIPTEN__
    downloadFile(logFileName(), logText().toUtf8());
    return {};
#else
    return tr("Downloads are only available in a browser");
#endif
}

} // namespace peerchalk::sync
