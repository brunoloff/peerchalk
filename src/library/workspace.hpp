#pragma once

#include "library/access_log.hpp"
#include "library/codes.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QUuid>

#include <optional>
#include <vector>

namespace peerchalk {

// A project this device knows, with its signed access changes and their result. After
// joining by invite, its changes arrive from peers; until then only the invite is known.
struct ProjectEntry {
    QUuid id;
    QByteArray capability; // discovery secret of the project's members
    QByteArray creator;    // trust anchor: the project's Create must be signed by this key
    QString inviteName;
    int colorIndex{0};
    std::vector<SignedOp> ops;
    AccessState state;

    [[nodiscard]] QString name() const { return state.exists ? state.name : inviteName; }
    [[nodiscard]] bool deleted() const { return state.deleted; }
};

// A board this device knows: created here, joined by a board invite, or learned through
// a project it belongs to (then without its own capability, and anchored by that project).
struct BoardEntry {
    QUuid id;
    QByteArray capability; // empty when known only through projects
    QByteArray creator;    // empty when learned through a project
    QString inviteName;
    QDateTime edited;
    std::vector<SignedOp> ops;
    AccessState state;
    QByteArray linkToken{}; // the secret of the board's link, if known here

    [[nodiscard]] QString name() const { return state.exists ? state.name : inviteName; }
    [[nodiscard]] bool deleted() const { return state.deleted; }
};

// This device's projects and boards and who may do what with them.
//
// Every project and board has an admin, owners and members (see access_log.hpp for who
// may change what). A project is a collection of boards: its members are members of all
// its boards; its owners add and remove boards they also own. A user may access a project
// they have a role on, and a board they have a role on or that belongs to a project they
// may access. Members may edit a board while one of its owners is connected.
//
// Data lives in `directory`: workspace.json (owner-only; it holds capabilities) and
// boards/<id>.yrs. Files from earlier versions are migrated on first load. Failures to
// read or write throw std::runtime_error; misuse (unknown ids, missing rights, empty
// names) throws std::invalid_argument. Changes are saved before returning.
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

    // Roles held directly on a project or board (not through projects).
    [[nodiscard]] Role projectRole(const QUuid &id, const QByteArray &user) const;
    [[nodiscard]] Role boardRole(const QUuid &id, const QByteArray &user) const;
    [[nodiscard]] bool mayAccessProject(const QByteArray &user, const QUuid &projectId) const;
    [[nodiscard]] bool mayAccessBoard(const QByteArray &user, const QUuid &boardId) const;
    // An owner of the board, or of a project it is in. Members can see and change a board
    // only while one of these is connected.
    [[nodiscard]] bool isBoardOwner(const QByteArray &user, const QUuid &boardId) const;
    // Boards shown in a project, and boards shown in none of your projects: accessible
    // ones, and ones you joined by invite (which may still await approval).
    [[nodiscard]] std::vector<QUuid> boardsInProject(const QUuid &projectId) const;
    [[nodiscard]] std::vector<QUuid> sharedBoards() const;

    // displayName is recorded as yours in the new project's or board's changes.
    QUuid createProject(const QString &name, const QString &displayName);
    // A board you administer, added to the given project if any (you must own it).
    QUuid createBoard(const QString &name, const std::optional<QUuid> &projectId,
                      const QString &displayName);
    void renameProject(const QUuid &id, const QString &name);
    void renameBoard(const QUuid &id, const QString &name);
    // Gives a user a role (Member or Owner) or removes them (None), as your role allows.
    void setProjectRole(const QUuid &id, const QByteArray &user, const QString &name, Role role);
    void setBoardRole(const QUuid &id, const QByteArray &user, const QString &name, Role role);
    // Makes someone else the admin; you stay an owner.
    void transferProjectAdmin(const QUuid &id, const QByteArray &user, const QString &name);
    void transferBoardAdmin(const QUuid &id, const QByteArray &user, const QString &name);
    void setBoardInProject(const QUuid &boardId, const QUuid &projectId, bool included);
    // As admin, deletes for everyone; otherwise forgets the project or board here.
    void removeProject(const QUuid &id);
    void removeBoard(const QUuid &id);

    // An invite that lets its receiver find the others and ask to join; for owners.
    [[nodiscard]] Invite inviteToProject(const QUuid &id) const;
    [[nodiscard]] Invite inviteToBoard(const QUuid &id) const;
    // Records an invite; throws std::invalid_argument if the item is already here.
    void join(const Invite &invite);

    // Board links, for owners: making a new one replaces the old one.
    BoardLink createBoardLink(const QUuid &id);
    // The working link, if this device knows its secret.
    [[nodiscard]] std::optional<BoardLink> currentBoardLink(const QUuid &id) const;
    [[nodiscard]] bool boardHasLink(const QUuid &id) const;
    void disableBoardLink(const QUuid &id);
    // Records a link you were given; an owner still has to admit you.
    void joinByLink(const BoardLink &link);
    // As an owner, makes someone a member if their token matches the link and they were
    // not removed from the board. Returns whether they were admitted.
    bool admitByLink(const QUuid &id, const QByteArray &user, const QString &name,
                     const QByteArray &token);

    // Takes in access changes from a peer: changes to known projects and boards, and to
    // boards listed by a project you may access. Returns true when something changed.
    bool acceptOps(const std::vector<std::pair<QByteArray, QByteArray>> &signedOps);
    // The changes to offer a peer that reached you through a project's or board's
    // capability (projects include their boards' changes), or all of them.
    [[nodiscard]] std::vector<SignedOp> opsForProject(const QUuid &projectId) const;
    [[nodiscard]] std::vector<SignedOp> opsForBoard(const QUuid &boardId) const;
    [[nodiscard]] std::vector<SignedOp> allOps() const;

    // Everything about projects and boards, including capabilities, for your own devices;
    // merging adds what is missing. Returns true when changed. Throws std::runtime_error
    // for malformed input.
    [[nodiscard]] QJsonObject exportPersonal() const;
    bool mergePersonal(const QJsonObject &personal);

    [[nodiscard]] QByteArray boardState(const QUuid &id) const;
    void saveBoardState(const QUuid &id, const QByteArray &state);

  private:
    ProjectEntry &findProject(const QUuid &id);
    void recordBoardLink(const QUuid &id, const QByteArray &hash);
    BoardEntry &findBoard(const QUuid &id);
    // Signs a change with the next clock after `clock`, adds it and recomputes.
    void record(std::vector<SignedOp> &ops, quint64 clock, AccessOp op);
    // Replays every entry: boards first, since projects check board roles.
    void recompute();
    bool addOp(const SignedOp &op);
    bool learnBoard(const QUuid &boardId, const std::vector<SignedOp> &boardOps);
    [[nodiscard]] QString boardPath(const QUuid &id) const;
    void load();
    bool migrate();
    // Re-creates a project or board you administered as signed changes.
    void recreate(ResourceKind kind, const QUuid &id, const QString &name,
                  const std::vector<std::pair<QByteArray, QString>> &members, bool deleted);
    void save() const;

    QString directory_;
    QByteArray user_;
    Signer signer_;
    std::vector<ProjectEntry> projects_;
    std::vector<BoardEntry> boards_;
};

} // namespace peerchalk
