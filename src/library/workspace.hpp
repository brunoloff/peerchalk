#pragma once

#include "library/codes.hpp"
#include "library/manifest.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QUuid>

#include <optional>
#include <vector>

namespace peerchalk {

// A project this device knows. Until its signed manifest arrives from a peer (after
// joining by invite) only the invite's details are known.
struct ProjectEntry {
    QUuid id;
    QByteArray capability; // discovery secret of the project's members
    QByteArray admin;      // trust anchor: the manifest must be signed by this key
    QString inviteName;
    int colorIndex{0};
    std::optional<SignedManifest> manifest;

    [[nodiscard]] QString name() const;
    [[nodiscard]] bool deleted() const { return manifest && manifest->manifest.deleted; }
};

// A board this device knows: created here, joined by a board invite, or learned through
// a project it belongs to (then without its own capability).
struct BoardEntry {
    QUuid id;
    QByteArray capability; // empty when known only through projects
    QByteArray admin;
    QString inviteName;
    std::optional<SignedManifest> manifest;
    QDateTime edited;

    [[nodiscard]] QString name() const;
    [[nodiscard]] bool deleted() const { return manifest && manifest->manifest.deleted; }
};

// This device's projects and boards and who may access them. Each project and board has
// one admin, whose signed manifest lists its members (and, for a board, the projects it
// belongs to). A user may access a project if its manifest includes them, and a board if
// its manifest includes them or it belongs to a project that includes both them and the
// board's admin. Only the admin changes a manifest; changes bump its version.
//
// Data lives in `directory`: workspace.json (owner-only; it holds capabilities) and
// boards/<id>.yrs. A projects.json from earlier versions is migrated on first load.
// Failures to read or write throw std::runtime_error; misuse (unknown ids, not admin,
// empty names) throws std::invalid_argument. Changes are saved before returning.
class Workspace {
  public:
    Workspace(QString directory, QByteArray user, Signer signer);

    [[nodiscard]] const std::vector<ProjectEntry> &projects() const { return projects_; }
    [[nodiscard]] const std::vector<BoardEntry> &boards() const { return boards_; }
    [[nodiscard]] const ProjectEntry &project(const QUuid &id) const;
    [[nodiscard]] const BoardEntry &board(const QUuid &id) const;
    [[nodiscard]] bool hasProject(const QUuid &id) const;
    [[nodiscard]] bool hasBoard(const QUuid &id) const;
    [[nodiscard]] QByteArray user() const { return user_; }
    // Switches to another user key (after linking this device to an existing identity);
    // throws std::invalid_argument unless the workspace is empty.
    void resetUser(QByteArray user);

    // Boards shown in a project, and accessible boards shown in none of your projects.
    [[nodiscard]] std::vector<QUuid> boardsInProject(const QUuid &projectId) const;
    [[nodiscard]] std::vector<QUuid> sharedBoards() const;
    [[nodiscard]] bool mayAccessProject(const QByteArray &user, const QUuid &projectId) const;
    [[nodiscard]] bool mayAccessBoard(const QByteArray &user, const QUuid &boardId) const;

    QUuid createProject(const QString &name);
    // Creates a board you administer, in the given project if any (you must be in it).
    QUuid createBoard(const QString &name, const std::optional<QUuid> &projectId);
    void renameProject(const QUuid &id, const QString &name);
    void renameBoard(const QUuid &id, const QString &name);
    void setProjectMembers(const QUuid &id, std::vector<Member> members);
    void setBoardMembers(const QUuid &id, std::vector<Member> members);
    // Adds or removes a board you administer to or from a project you are in.
    void setBoardInProject(const QUuid &boardId, const QUuid &projectId, bool included);
    // As admin, publishes a deletion; otherwise forgets the project or board here.
    void removeProject(const QUuid &id);
    void removeBoard(const QUuid &id);

    // An invite that lets its receiver find the other members (admin only for projects
    // and boards with their own capability).
    [[nodiscard]] Invite inviteToProject(const QUuid &id) const;
    [[nodiscard]] Invite inviteToBoard(const QUuid &id) const;
    // Records an invite; throws std::invalid_argument if the item is already here.
    void join(const Invite &invite);

    // Accepts a manifest a peer sent if it is valid and newer than the one held: for a
    // known project or board it must be signed by that entry's admin; an unknown board is
    // accepted when it belongs to a project you are in whose manifest includes the
    // board's admin. Returns true when something changed.
    bool acceptManifest(const QByteArray &payload, const QByteArray &signature);
    // The signed manifests to offer a peer that reached you through `projectId` or
    // `boardId`'s capability (or all of them, for your own devices).
    [[nodiscard]] std::vector<SignedManifest> manifestsForProject(const QUuid &projectId) const;
    [[nodiscard]] std::vector<SignedManifest> manifestsForBoard(const QUuid &boardId) const;
    [[nodiscard]] std::vector<SignedManifest> allManifests() const;

    // Everything about projects and boards, including capabilities, for your own devices;
    // merging adds what is missing and newer manifests. Returns true when changed.
    [[nodiscard]] QJsonObject exportPersonal() const;
    bool mergePersonal(const QJsonObject &personal);

    [[nodiscard]] QByteArray boardState(const QUuid &id) const;
    void saveBoardState(const QUuid &id, const QByteArray &state);

  private:
    ProjectEntry &findProject(const QUuid &id);
    BoardEntry &findBoard(const QUuid &id);
    [[nodiscard]] Manifest adminManifest(const std::optional<SignedManifest> &current,
                                         const QByteArray &admin) const;
    void publish(std::optional<SignedManifest> &slot, const Manifest &manifest);
    [[nodiscard]] QString boardPath(const QUuid &id) const;
    bool acceptBoardManifest(const SignedManifest &signedManifest);
    void load();
    void migrateProjectsFile();
    void save() const;

    QString directory_;
    QByteArray user_;
    Signer signer_;
    std::vector<ProjectEntry> projects_;
    std::vector<BoardEntry> boards_;
};

} // namespace peerchalk
