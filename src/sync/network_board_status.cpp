// Network: what the board's network pane shows, for the board that is open.
#include "sync/board_diagnosis.hpp"
#include "sync/network.hpp"
#include "sync/network_settings.hpp"
#include "sync/repository_scribe.hpp"
#include "sync/server_list.hpp"
#include "ui/library.hpp"

#include <algorithm>
#include <set>

namespace peerchalk::sync {
namespace {

constexpr bool inBrowser =
#ifdef __EMSCRIPTEN__
    true;
#else
    false;
#endif

QString hexOf(const QByteArray &bytes) { return QString::fromLatin1(bytes.toHex()); }

QByteArray keyBytes(const p2pdc::SigningPublicKey &key) {
    return {reinterpret_cast<const char *>(key.data()), static_cast<qsizetype>(key.size())};
}

// Counts the servers of a group by their last check into facts.
void countServers(const std::vector<ServerEntry> &entries, const QVariantMap &status, bool online,
                  BoardConnectionFacts &facts) {
    for (const ServerEntry &entry : entries) {
        const QString key =
            QLatin1String(serverKindName(entry.kind)) + QLatin1Char(' ') + entry.address;
        QString state = status.value(key).toMap().value(QStringLiteral("state")).toString();
        if (entry.kind == ServerKind::Turn) {
            facts.relayConfigured = true;
            facts.relayWorking = facts.relayWorking || state == QStringLiteral("working");
            continue;
        }
        // On desktops the DHT works once the node is online (as NetworkSummary.qml says).
        if (entry.kind == ServerKind::Dht && !inBrowser && state != QStringLiteral("unreachable"))
            state = online ? QStringLiteral("working") : QString();
        if (state == QStringLiteral("working"))
            ++facts.discoveryWorking;
        else if (state == QStringLiteral("unreachable"))
            ++facts.discoveryUnreachable;
        else
            ++facts.discoveryChecking;
    }
}

QVariantMap factsMap(const BoardConnectionFacts &facts) {
    return {
        {QStringLiteral("discoveryWorking"), facts.discoveryWorking},
        {QStringLiteral("discoveryUnreachable"), facts.discoveryUnreachable},
        {QStringLiteral("discoveryChecking"), facts.discoveryChecking},
        {QStringLiteral("relayConfigured"), facts.relayConfigured},
        {QStringLiteral("relayWorking"), facts.relayWorking},
        {QStringLiteral("announced"), facts.announced},
        {QStringLiteral("seen"), facts.seen},
        {QStringLiteral("connecting"), facts.connecting},
        {QStringLiteral("connectingSeconds"), static_cast<int>(facts.longestConnectingMs / 1000)},
        {QStringLiteral("direct"), facts.direct},
        {QStringLiteral("relayed"), facts.relayed}};
}

// A member's best connection: here (on this board) beats connected beats connecting.
int rank(const QString &state) {
    return state == QStringLiteral("here")         ? 3
           : state == QStringLiteral("connected")  ? 2
           : state == QStringLiteral("connecting") ? 1
                                                   : 0;
}

} // namespace

// The parts of Network::boardConnection that read Network's connections.
struct BoardStatus {
    // Everyone with a role on the board, each {key, name, role, you, colorIndex, state, route,
    // milliseconds}: state "here", "connected", "connecting" or "away"; counts connections
    // into facts.
    static QVariantList people(const Network &network, const QUuid &board,
                               BoardConnectionFacts &facts) {
        std::map<QString, QVariantMap> byUser = members(network, board);
        std::set<p2pdc::SigningPublicKey> counted;
        for (const auto &[id, peer] : network.peers_) {
            if (!network.sharesBoard(*peer, board) || !counted.insert(peer->device).second)
                continue;
            const bool ready = peer->ready && peer->user;
            if (!ready) {
                ++facts.connecting;
                facts.longestConnectingMs =
                    std::max(facts.longestConnectingMs, peer->started.elapsed());
            } else if (peer->routeKind == p2pdc::RouteKind::TurnRelayed) {
                ++facts.relayed;
            } else {
                ++facts.direct;
            }
            if (peer->user)
                if (const auto person = byUser.find(hexOf(peer->user->userKey));
                    person != byUser.end())
                    note(*peer, board, person->second);
        }
        QVariantList result;
        for (auto &[key, person] : byUser)
            result.append(person);
        std::ranges::stable_sort(result, [](const QVariant &a, const QVariant &b) {
            return rank(a.toMap().value(QStringLiteral("state")).toString()) >
                   rank(b.toMap().value(QStringLiteral("state")).toString());
        });
        return result;
    }

    static std::map<QString, QVariantMap> members(const Network &network, const QUuid &board) {
        std::map<QString, QVariantMap> byUser;
        if (!network.library_.workspace().hasBoard(board))
            return byUser; // boardPeople would report an unknown board as an error
        for (const QVariant &value :
             network.library_.boardPeople(board.toString(QUuid::WithoutBraces))) {
            QVariantMap person = value.toMap();
            if (person.value(QStringLiteral("role")).toString() == QStringLiteral("none"))
                continue;
            person.insert(QStringLiteral("state"), QStringLiteral("away"));
            byUser[person.value(QStringLiteral("key")).toString()] = person;
        }
        return byUser;
    }

    // Records how a member is connected through peer, unless they are better connected.
    static void note(const Network::Peer &peer, const QUuid &board, QVariantMap &person) {
        const bool ready = peer.ready && peer.user;
        const QString state = !ready ? QStringLiteral("connecting")
                              : peer.presence && peer.presence->board == board
                                  ? QStringLiteral("here")
                                  : QStringLiteral("connected");
        if (rank(state) <= rank(person.value(QStringLiteral("state")).toString()))
            return;
        const bool relayed = peer.routeKind == p2pdc::RouteKind::TurnRelayed;
        person.insert(QStringLiteral("state"), state);
        person.insert(QStringLiteral("colorIndex"), peer.user->colorIndex);
        person.insert(QStringLiteral("route"),
                      relayed ? QStringLiteral("relay") : QStringLiteral("direct"));
        if (ready && peer.roundTrip)
            person.insert(QStringLiteral("milliseconds"),
                          static_cast<int>(peer.roundTrip->count()));
    }

    // For a repository board open here: {open, scribe (this device commits), hasScribe,
    // scribeName (who does, if another), pushedAt (ISO time of the last push heard of)}.
    static QVariantMap repository(const Network &network, const QUuid &board) {
        const RepositoryScribe &scribes = *network.repository_;
        const std::vector<QUuid> local = scribes.localBoards();
        if (std::ranges::find(local, board) == local.end())
            return {{QStringLiteral("open"), false}};
        const std::optional<QByteArray> scribe = scribes.scribe(board);
        QString scribeName;
        for (const auto &[id, peer] : network.peers_)
            if (scribe && peer->user && keyBytes(peer->device) == *scribe)
                scribeName = peer->user->name;
        const QDateTime pushedAt = scribes.lastPushedAt(board);
        return {{QStringLiteral("open"), true},
                {QStringLiteral("scribe"), scribes.isScribe(board)},
                {QStringLiteral("hasScribe"), scribe.has_value()},
                {QStringLiteral("scribeName"), scribeName},
                {QStringLiteral("pushedAt"),
                 pushedAt.isValid() ? pushedAt.toString(Qt::ISODate) : QString()}};
    }
};

QVariantMap Network::boardConnection(const QString &boardId) const {
    const QUuid board = QUuid::fromString(boardId);
    BoardConnectionFacts facts;
    facts.online = online();
    facts.updateNeeded = updateNeeded_;
    try {
        const ServerList list(NetworkSettings::load(library_.directory()).withEnvironment(),
                              inBrowser);
        countServers(list.entries(ServerGroup::Discovery), serverStatus_, facts.online, facts);
        countServers(list.entries(ServerGroup::Firewall), serverStatus_, facts.online, facts);
    } catch (const std::exception &error) {
        qWarning("PeerChalk could not read the network settings: %s", error.what());
    }
    // The spaces this board is found in: its own, and those of projects it belongs to.
    std::set<QString> keys{QStringLiteral("b:") + board.toString(QUuid::WithoutBraces)};
    for (const ProjectEntry &project : library_.workspace().projects()) {
        const auto boards = library_.workspace().boardsInProject(project.id);
        if (std::ranges::find(boards, board) != boards.end())
            keys.insert(QStringLiteral("p:") + project.id.toString(QUuid::WithoutBraces));
    }
    std::set<p2pdc::SigningPublicKey> seen;
    for (const QString &key : keys) {
        if (const auto space = spaces_.find(key); space != spaces_.end()) {
            facts.announced = facts.announced || space->second->published;
            for (const auto &[device, advertisement] : space->second->advertisements)
                seen.insert(device);
        }
    }
    facts.seen = static_cast<int>(seen.size());
    QVariantMap result{{QStringLiteral("people"), BoardStatus::people(*this, board, facts)},
                       {QStringLiteral("repository"), BoardStatus::repository(*this, board)}};
    result.insert(QStringLiteral("state"), stateName(diagnose(facts)));
    result.insert(QStringLiteral("facts"), factsMap(facts));
    return result;
}

} // namespace peerchalk::sync
