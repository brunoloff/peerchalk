// Network: which discovery spaces are open.
#include "sync/connection_log.hpp"
#include "sync/network.hpp"
#include "sync/space_schedule.hpp"

#include "ui/library.hpp"

#include <algorithm>

namespace peerchalk::sync {

using namespace p2pdc;

namespace {

constexpr std::size_t maxSpaces = 16; // p2pdc's limit on subscriptions per node
constexpr std::size_t rotatingSpaces = 4;
// p2pdc admits 16 announcements per 5 minutes; wait a little when that is used up.
constexpr auto openRetryDelay = std::chrono::seconds(30);

template <class T> bool includes(const std::vector<T> &items, const T &item) {
    return std::ranges::find(items, item) != items.end();
}

QString projectKey(const QUuid &id) {
    return QStringLiteral("p:") + id.toString(QUuid::WithoutBraces);
}

QString boardKey(const QUuid &id) {
    return QStringLiteral("b:") + id.toString(QUuid::WithoutBraces);
}

DiscoveryCapability toCapability(const QByteArray &bytes) {
    DiscoveryCapability capability{};
    std::copy_n(reinterpret_cast<const std::byte *>(bytes.constData()), capability.size(),
                capability.begin());
    return capability;
}

QDateTime lastEditedIn(const Workspace &workspace, const QUuid &project) {
    QDateTime latest;
    for (const QUuid &id : workspace.boardsInProject(project))
        if (workspace.hasBoard(id))
            latest = std::max(latest, workspace.board(id).edited);
    return latest;
}

} // namespace

std::map<QString, Network::SpaceSpec> Network::wantedSpaces() const {
    std::map<QString, SpaceSpec> wanted;
    const Workspace &workspace = library_.workspace();
    if (config_.personalSpace)
        wanted[QStringLiteral("personal")] = {
            SpaceKind::Personal,
            QUuid(),
            library_.identityStore().identity().personalCapability(),
            {}};
    for (const ProjectEntry &project : workspace.projects())
        if (!project.deleted())
            wanted[projectKey(project.id)] = {SpaceKind::Project, project.id, project.capability,
                                              lastEditedIn(workspace, project.id)};
    for (const BoardEntry &board : workspace.boards())
        if (!board.deleted() && board.capability.size() == 32)
            wanted[boardKey(board.id)] = {SpaceKind::Board, board.id, board.capability,
                                          board.edited};
    return wanted;
}

// Your own devices, the project on screen and the board that is open.
std::set<QString> Network::essentialSpaces() const {
    std::set<QString> keys;
    if (config_.personalSpace)
        keys.insert(QStringLiteral("personal"));
    const QUuid current = QUuid::fromString(library_.currentProjectId());
    if (!current.isNull())
        keys.insert(projectKey(current));
    if (!presenceBoard_.isNull()) {
        keys.insert(boardKey(presenceBoard_));
        const Workspace &workspace = library_.workspace();
        for (const ProjectEntry &project : workspace.projects())
            if (includes(workspace.boardsInProject(project.id), presenceBoard_))
                keys.insert(projectKey(project.id));
    }
    return keys;
}

// Opens the spaces worth watching now and closes the others.
void Network::reconcileSpaces() {
    if (!started_ || stopping_)
        return;
    const std::map<QString, SpaceSpec> wanted = wantedSpaces();
    const std::set<QString> essential = essentialSpaces();
    std::set<QString> connected;
    for (const auto &[id, peer] : peers_)
        if (peer->ready)
            connected.insert(id.first);
    std::vector<SpaceCandidate> candidates;
    for (const auto &[key, spec] : wanted)
        candidates.push_back(
            {key, essential.contains(key), connected.contains(key), spec.lastEdited});
    const std::vector<QString> chosen =
        chooseSpaces(std::move(candidates), maxSpaces, rotatingSpaces, rotationStep_);
    std::vector<QString> obsolete;
    for (const auto &[key, space] : spaces_)
        if (!includes(chosen, key))
            obsolete.push_back(key);
    for (const QString &key : obsolete)
        closeSpace(key);
    if (!spaceOpenDelay_.hasExpired())
        return;
    for (const QString &key : chosen) {
        if (spaces_.contains(key))
            continue;
        const SpaceSpec &spec = wanted.at(key);
        if (!openSpace(key, spec.kind, spec.id, spec.capability)) {
            spaceOpenDelay_.setRemainingTime(openRetryDelay);
            return;
        }
    }
}

void Network::rotateSpaces() {
    if (!spaceOpenDelay_.hasExpired())
        return; // moving on now would close a space without being able to open another
    ++rotationStep_;
    reconcileSpaces();
}

bool Network::openSpace(const QString &key, SpaceKind kind, const QUuid &id,
                        const QByteArray &capability) {
    auto space = std::make_unique<Space>();
    space->kind = kind;
    space->id = id;
    space->capability = toCapability(capability);
    spaces_[key] = std::move(space);
    if (watch(key) && announce(key)) {
        logInfo(QStringLiteral("discovery"), QStringLiteral("%1: opened").arg(spaceLabel(key)));
        return true;
    }
    spaces_.erase(key); // retried by a later reconcile
    logWarning(QStringLiteral("discovery"),
               QStringLiteral("%1: could not open; retrying later").arg(spaceLabel(key)));
    return false;
}

void Network::closeSpace(const QString &key) {
    logInfo(QStringLiteral("discovery"), QStringLiteral("%1: closed").arg(spaceLabel(key)));
    std::erase_if(peers_, [&key](const auto &entry) { return entry.first.first == key; });
    spaces_.erase(key);
    emit peersChanged();
    emit presenceChanged();
}

} // namespace peerchalk::sync
