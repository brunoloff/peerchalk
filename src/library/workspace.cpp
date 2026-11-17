#include "library/workspace.hpp"

#include "library/files.hpp"
#include "library/secrets.hpp"

#include <QDir>

#include <algorithm>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr int projectColorCount = 6;

QString requireName(const QString &name) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        throw std::invalid_argument("Names cannot be empty");
    return trimmed;
}

template <class Entry> auto findIn(std::vector<Entry> &entries, const QUuid &id) {
    return std::ranges::find(entries, id, &Entry::id);
}

} // namespace

QString ProjectEntry::name() const { return manifest ? manifest->manifest.name : inviteName; }

QString BoardEntry::name() const { return manifest ? manifest->manifest.name : inviteName; }

Workspace::Workspace(QString directory, QByteArray user, Signer signer)
    : directory_(std::move(directory)), user_(std::move(user)), signer_(std::move(signer)) {
    load();
}

const ProjectEntry &Workspace::project(const QUuid &id) const {
    const auto it = std::ranges::find(projects_, id, &ProjectEntry::id);
    if (it == projects_.end())
        throw std::invalid_argument("Unknown project");
    return *it;
}

const BoardEntry &Workspace::board(const QUuid &id) const {
    const auto it = std::ranges::find(boards_, id, &BoardEntry::id);
    if (it == boards_.end())
        throw std::invalid_argument("Unknown board");
    return *it;
}

void Workspace::resetUser(QByteArray user) {
    if (!projects_.empty() || !boards_.empty())
        throw std::invalid_argument("This device already has projects or boards");
    user_ = std::move(user);
}

bool Workspace::hasProject(const QUuid &id) const {
    return std::ranges::find(projects_, id, &ProjectEntry::id) != projects_.end();
}

bool Workspace::hasBoard(const QUuid &id) const {
    return std::ranges::find(boards_, id, &BoardEntry::id) != boards_.end();
}

ProjectEntry &Workspace::findProject(const QUuid &id) {
    return const_cast<ProjectEntry &>(std::as_const(*this).project(id));
}

BoardEntry &Workspace::findBoard(const QUuid &id) {
    return const_cast<BoardEntry &>(std::as_const(*this).board(id));
}

std::vector<QUuid> Workspace::boardsInProject(const QUuid &projectId) const {
    std::vector<QUuid> result;
    if (!hasProject(projectId))
        return result;
    const ProjectEntry &project = this->project(projectId);
    if (!project.manifest || project.deleted())
        return result;
    for (const BoardEntry &board : boards_) {
        if (!board.manifest || board.deleted())
            continue;
        const Manifest &manifest = board.manifest->manifest;
        if (std::ranges::find(manifest.projects, projectId) != manifest.projects.end() &&
            project.manifest->manifest.includes(manifest.admin))
            result.push_back(board.id);
    }
    return result;
}

std::vector<QUuid> Workspace::sharedBoards() const {
    std::vector<QUuid> inProjects;
    for (const ProjectEntry &project : projects_) {
        if (mayAccessProject(user_, project.id))
            std::ranges::copy(boardsInProject(project.id), std::back_inserter(inProjects));
    }
    std::vector<QUuid> result;
    for (const BoardEntry &board : boards_) {
        const bool shown = std::ranges::find(inProjects, board.id) != inProjects.end();
        if (!shown && !board.deleted() && (!board.manifest || mayAccessBoard(user_, board.id)))
            result.push_back(board.id);
    }
    return result;
}

bool Workspace::mayAccessProject(const QByteArray &user, const QUuid &projectId) const {
    if (!hasProject(projectId))
        return false;
    const ProjectEntry &project = this->project(projectId);
    return project.manifest && !project.deleted() && project.manifest->manifest.includes(user);
}

bool Workspace::mayAccessBoard(const QByteArray &user, const QUuid &boardId) const {
    if (!hasBoard(boardId))
        return false;
    const BoardEntry &board = this->board(boardId);
    if (!board.manifest || board.deleted())
        return false;
    const Manifest &manifest = board.manifest->manifest;
    if (manifest.includes(user))
        return true;
    return std::ranges::any_of(manifest.projects, [&](const QUuid &projectId) {
        return mayAccessProject(user, projectId) &&
               project(projectId).manifest->manifest.includes(manifest.admin);
    });
}

Manifest Workspace::adminManifest(const std::optional<SignedManifest> &current,
                                  const QByteArray &admin) const {
    if (admin != user_ || !current)
        throw std::invalid_argument("Only the admin can change this");
    Manifest next = current->manifest;
    ++next.version;
    return next;
}

void Workspace::publish(std::optional<SignedManifest> &slot, const Manifest &manifest) {
    slot = signManifest(manifest, signer_);
    save();
}

QUuid Workspace::createProject(const QString &name) {
    ProjectEntry entry{QUuid::createUuid(),
                       newCapability(),
                       user_,
                       requireName(name),
                       static_cast<int>(projects_.size()) % projectColorCount,
                       std::nullopt};
    projects_.push_back(entry);
    publish(projects_.back().manifest,
            Manifest{ResourceKind::Project, entry.id, entry.inviteName, user_, 1, {}, {}, false});
    return entry.id;
}

QUuid Workspace::createBoard(const QString &name, const std::optional<QUuid> &projectId) {
    std::vector<QUuid> projects;
    if (projectId) {
        if (!mayAccessProject(user_, *projectId))
            throw std::invalid_argument("You are not in that project");
        projects.push_back(*projectId);
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    BoardEntry entry{QUuid::createUuid(), newCapability(), user_,
                     requireName(name),   std::nullopt,    now};
    boards_.push_back(entry);
    publish(
        boards_.back().manifest,
        Manifest{ResourceKind::Board, entry.id, entry.inviteName, user_, 1, {}, projects, false});
    return entry.id;
}

void Workspace::renameProject(const QUuid &id, const QString &name) {
    ProjectEntry &entry = findProject(id);
    Manifest next = adminManifest(entry.manifest, entry.admin);
    next.name = requireName(name);
    publish(entry.manifest, next);
}

void Workspace::renameBoard(const QUuid &id, const QString &name) {
    BoardEntry &entry = findBoard(id);
    Manifest next = adminManifest(entry.manifest, entry.admin);
    next.name = requireName(name);
    publish(entry.manifest, next);
}

void Workspace::setProjectMembers(const QUuid &id, std::vector<Member> members) {
    ProjectEntry &entry = findProject(id);
    Manifest next = adminManifest(entry.manifest, entry.admin);
    std::erase_if(members, [&](const Member &m) { return m.key == entry.admin; });
    next.members = std::move(members);
    publish(entry.manifest, next);
}

void Workspace::setBoardMembers(const QUuid &id, std::vector<Member> members) {
    BoardEntry &entry = findBoard(id);
    Manifest next = adminManifest(entry.manifest, entry.admin);
    std::erase_if(members, [&](const Member &m) { return m.key == entry.admin; });
    next.members = std::move(members);
    publish(entry.manifest, next);
}

void Workspace::setBoardInProject(const QUuid &boardId, const QUuid &projectId, bool included) {
    BoardEntry &entry = findBoard(boardId);
    Manifest next = adminManifest(entry.manifest, entry.admin);
    std::erase(next.projects, projectId);
    if (included) {
        if (!mayAccessProject(user_, projectId))
            throw std::invalid_argument("You are not in that project");
        next.projects.push_back(projectId);
    }
    publish(entry.manifest, next);
}

void Workspace::removeProject(const QUuid &id) {
    ProjectEntry &entry = findProject(id);
    if (entry.admin == user_ && entry.manifest) {
        Manifest next = adminManifest(entry.manifest, entry.admin);
        next.deleted = true;
        publish(entry.manifest, next);
        return;
    }
    projects_.erase(findIn(projects_, id));
    save();
}

void Workspace::removeBoard(const QUuid &id) {
    BoardEntry &entry = findBoard(id);
    removeFile(boardPath(id));
    if (entry.admin == user_ && entry.manifest) {
        Manifest next = adminManifest(entry.manifest, entry.admin);
        next.deleted = true;
        publish(entry.manifest, next);
        return;
    }
    boards_.erase(findIn(boards_, id));
    save();
}

Invite Workspace::inviteToProject(const QUuid &id) const {
    const ProjectEntry &entry = project(id);
    if (entry.admin != user_)
        throw std::invalid_argument("Only the project's admin can invite people");
    return {Invite::Kind::Project, id, entry.name(), entry.capability, entry.admin};
}

Invite Workspace::inviteToBoard(const QUuid &id) const {
    const BoardEntry &entry = board(id);
    if (entry.admin != user_ || entry.capability.isEmpty())
        throw std::invalid_argument("Only the board's admin can invite people");
    return {Invite::Kind::Board, id, entry.name(), entry.capability, entry.admin};
}

void Workspace::join(const Invite &invite) {
    if (invite.capability.size() != capabilitySize || invite.admin.size() != signingKeySize)
        throw std::invalid_argument("The invite is damaged");
    if (invite.kind == Invite::Kind::Project) {
        if (hasProject(invite.id))
            throw std::invalid_argument("You are already in this project");
        projects_.push_back({invite.id, invite.capability, invite.admin, requireName(invite.name),
                             static_cast<int>(projects_.size()) % projectColorCount, std::nullopt});
    } else if (hasBoard(invite.id)) {
        BoardEntry &entry = findBoard(invite.id);
        if (!entry.capability.isEmpty())
            throw std::invalid_argument("You already have this board");
        entry.capability = invite.capability; // known through a project; now invited directly
    } else {
        boards_.push_back({invite.id, invite.capability, invite.admin, requireName(invite.name),
                           std::nullopt, QDateTime::currentDateTimeUtc()});
    }
    save();
}

QString Workspace::boardPath(const QUuid &id) const {
    return QDir(directory_)
        .filePath(QStringLiteral("boards/%1.yrs").arg(id.toString(QUuid::WithoutBraces)));
}

QByteArray Workspace::boardState(const QUuid &id) const {
    static_cast<void>(board(id)); // throws for unknown boards
    return readFile(boardPath(id)).value_or(QByteArray());
}

void Workspace::saveBoardState(const QUuid &id, const QByteArray &state) {
    BoardEntry &entry = findBoard(id);
    writeFile(boardPath(id), state, true);
    entry.edited = QDateTime::currentDateTimeUtc();
    save();
}

} // namespace peerchalk
