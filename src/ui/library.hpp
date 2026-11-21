#pragma once

#include "library/identity_store.hpp"
#include "library/workspace.hpp"
#include "ui/board_document.hpp"
#include "ui/row_model.hpp"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <map>

class QQmlEngine;
class QJSEngine;
class QTimer;

namespace peerchalk {

// One account's projects, boards, identity and contacts for QML. Its folder comes from
// the engine's accountDirectory property, or else is defaultDataDirectory(). Operations that fail
// emit errorOccurred with a message for the user and return an empty value instead of throwing into
// QML.
//
// The board list shows the current selection: a project, or `sharedId` for boards
// shared with you directly.
class Library : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(peerchalk::RowModel *projects READ projects CONSTANT)
    Q_PROPERTY(peerchalk::RowModel *boards READ boards CONSTANT)
    Q_PROPERTY(peerchalk::RowModel *contacts READ contacts CONSTANT)
    Q_PROPERTY(QString sharedId READ sharedId CONSTANT)
    Q_PROPERTY(int sharedBoardCount READ sharedBoardCount NOTIFY workspaceChanged)
    Q_PROPERTY(QString currentProjectId READ currentProjectId WRITE setCurrentProjectId NOTIFY
                   currentProjectChanged)
    Q_PROPERTY(QString currentProjectName READ currentProjectName NOTIFY currentProjectChanged)
    Q_PROPERTY(bool currentProjectIsAdmin READ currentProjectIsAdmin NOTIFY currentProjectChanged)
    Q_PROPERTY(bool currentProjectPending READ currentProjectPending NOTIFY currentProjectChanged)
    Q_PROPERTY(QString boardFilter READ boardFilter WRITE setBoardFilter NOTIFY boardFilterChanged)
    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName NOTIFY profileChanged)
    Q_PROPERTY(int colorIndex READ colorIndex WRITE setColorIndex NOTIFY profileChanged)
    // Pen colours picked with the custom colour button, most recent first.
    Q_PROPERTY(QStringList customColors READ customColors NOTIFY customColorsChanged)
    Q_PROPERTY(QString fingerprint READ fingerprint NOTIFY profileChanged)
    Q_PROPERTY(QString userKey READ userKey NOTIFY profileChanged)
    Q_PROPERTY(QString contactCode READ contactCode NOTIFY profileChanged)
    Q_PROPERTY(QString deviceName READ deviceName CONSTANT)
    Q_PROPERTY(bool canAdoptIdentity READ canAdoptIdentity NOTIFY workspaceChanged)
    // Changes with every workspace change, for QML bindings that depend on it.
    Q_PROPERTY(int workspaceRevision READ workspaceRevision NOTIFY workspaceChanged)

  public:
    // Throws std::runtime_error when the stored data cannot be read.
    explicit Library(const QString &directory, QObject *parent = nullptr);
    // Saves boards with unsaved changes.
    ~Library() override;
    static Library *create(QQmlEngine *engine, QJSEngine *scriptEngine);
    // PEERCHALK_DATA_DIR, or else the platform's app data folder; accounts live inside it.
    [[nodiscard]] static QString defaultDataDirectory();

    [[nodiscard]] RowModel *projects() const { return projects_; }
    [[nodiscard]] RowModel *boards() const { return boards_; }
    [[nodiscard]] RowModel *contacts() const { return contacts_; }
    [[nodiscard]] static QString sharedId() { return QStringLiteral("shared"); }
    [[nodiscard]] int sharedBoardCount() const;
    [[nodiscard]] QString currentProjectId() const { return currentProject_; }
    void setCurrentProjectId(const QString &id);
    [[nodiscard]] QString currentProjectName() const;
    [[nodiscard]] bool currentProjectIsAdmin() const;
    [[nodiscard]] bool currentProjectPending() const;
    [[nodiscard]] QString boardFilter() const { return boardFilter_; }
    void setBoardFilter(const QString &filter);
    [[nodiscard]] QString displayName() const { return identity_.displayName(); }
    void setDisplayName(const QString &name);
    [[nodiscard]] int colorIndex() const { return identity_.colorIndex(); }
    void setColorIndex(int index);
    [[nodiscard]] QStringList customColors() const { return identity_.customColors(); }
    [[nodiscard]] QString fingerprint() const;
    [[nodiscard]] QString userKey() const;
    [[nodiscard]] QString contactCode() const;
    [[nodiscard]] QString deviceName() const { return identity_.identity().deviceName(); }
    [[nodiscard]] bool canAdoptIdentity() const;
    [[nodiscard]] int workspaceRevision() const { return workspaceRevision_; }

    // The data folder this library reads and writes.
    [[nodiscard]] QString directory() const { return directory_; }
    // Told about the strokes being drawn on any board of this library (for sync).
    using DraftListener = std::function<void(const QUuid &, const Stroke &, DraftState)>;
    void setDraftListener(DraftListener listener) { draftListener_ = std::move(listener); }
    [[nodiscard]] Workspace &workspace() { return workspace_; }
    [[nodiscard]] IdentityStore &identityStore() { return identity_; }

    Q_INVOKABLE QString createProject(const QString &name);
    // Joins the project or board in an invite code; returns its id, or empty on failure.
    Q_INVOKABLE QString joinInvite(const QString &code);
    Q_INVOKABLE QString projectInvite(const QString &projectId);
    Q_INVOKABLE QString boardInvite(const QString &boardId);
    // Board links: the working one ("" if none is known here), a new one replacing it, and
    // turning links off. siteUrl is where the app is hosted (see Accounts.siteUrl).
    Q_INVOKABLE QString boardLink(const QString &boardId, const QString &siteUrl);
    Q_INVOKABLE QString createBoardLink(const QString &boardId, const QString &siteUrl);
    Q_INVOKABLE void disableBoardLink(const QString &boardId);
    // Joins through a link if needed and returns the board's id, or "" on failure.
    Q_INVOKABLE QString openBoardLink(const QString &url);
    Q_INVOKABLE void renameProject(const QString &projectId, const QString &name);
    Q_INVOKABLE void removeProject(const QString &projectId);
    Q_INVOKABLE bool isProjectAdmin(const QString &projectId);

    // Boards are created in the current project (or on their own under "shared").
    Q_INVOKABLE QString createBoard(const QString &name);
    // A page board with one column of one page. paper is "A4", "A5", "Letter" or "Legal";
    // background is "blank", "ruled", "squared" or "dotted". Returns "" on failure.
    Q_INVOKABLE QString createPageBoard(const QString &name, const QString &paper, bool landscape,
                                        const QString &background);
    Q_INVOKABLE void renameBoard(const QString &boardId, const QString &name);
    Q_INVOKABLE void removeBoard(const QString &boardId);
    Q_INVOKABLE bool isBoardAdmin(const QString &boardId);
    Q_INVOKABLE QString boardName(const QString &boardId);
    Q_INVOKABLE QByteArray boardState(const QString &boardId);
    void saveBoardState(const QString &boardId, const QByteArray &state);
    // The live document of a board: loaded from disk on first use, shared by every view
    // and peer connection, and saved shortly after each change. Null for unknown boards.
    Q_INVOKABLE peerchalk::BoardDocument *openBoard(const QString &boardId);
    // Saves every board with unsaved changes now.
    Q_INVOKABLE void saveBoards();
    // The live document for C++ users such as sync; throws for unknown boards.
    BoardDocument &document(const QUuid &boardId);

    // Your role ("admin", "owner", "member" or "none") on a project or board itself.
    Q_INVOKABLE QString projectRole(const QString &projectId);
    Q_INVOKABLE QString boardRole(const QString &boardId);
    // The people with a role, then your other contacts (who can be added), each
    // {key, name, fingerprint, role, you}.
    Q_INVOKABLE QVariantList projectPeople(const QString &projectId);
    // How many people have a role in the project, you included.
    Q_INVOKABLE int projectMemberCount(const QString &projectId);
    Q_INVOKABLE QVariantList boardPeople(const QString &boardId);
    // role: "member", "owner" or "none" (remove), as your own role allows.
    Q_INVOKABLE void setProjectRole(const QString &projectId, const QString &keyHex,
                                    const QString &role);
    Q_INVOKABLE void setBoardRole(const QString &boardId, const QString &keyHex,
                                  const QString &role);
    Q_INVOKABLE void transferProjectAdmin(const QString &projectId, const QString &keyHex);
    Q_INVOKABLE void transferBoardAdmin(const QString &boardId, const QString &keyHex);
    // Lets in someone who asked to join (see Network.joinRequests): adds them to your
    // known people, unverified, if needed, and makes them a member.
    Q_INVOKABLE void admitToProject(const QString &projectId, const QString &keyHex,
                                    const QString &name);
    Q_INVOKABLE void admitToBoard(const QString &boardId, const QString &keyHex,
                                  const QString &name);
    // The projects you are in, each {projectId, name, included} for the board.
    Q_INVOKABLE QVariantList boardProjects(const QString &boardId);
    Q_INVOKABLE void setBoardInProject(const QString &boardId, const QString &projectId,
                                       bool included);

    // Adds the person in a contact code; returns false on failure.
    Q_INVOKABLE bool addContact(const QString &code);
    // how: "none", "in-person", "call" or "shared".
    Q_INVOKABLE void setVerification(const QString &keyHex, const QString &how);
    Q_INVOKABLE void removeContact(const QString &keyHex);
    // A secret code for linking another device to your identity.
    Q_INVOKABLE QString identityTransferCode();
    // Makes this device one of yours using a code from another device; only possible
    // before this device has any projects or boards.
    Q_INVOKABLE bool adoptIdentity(const QString &code);
    // Adds the people from a backup (see Accounts.importPeople) to your known people.
    Q_INVOKABLE void importPeople(const QVariantMap &people);
    // Remembers a pen colour ("#RRGGBB") you picked yourself.
    Q_INVOKABLE void addCustomColor(const QString &color);
    Q_INVOKABLE void copyToClipboard(const QString &text);

    // As an owner, admits someone who presented the board link's token (sync).
    bool admitByLink(const QUuid &board, const QByteArray &user, const QString &name,
                     const QByteArray &token);

    // Refreshes the lists after the workspace or contacts changed elsewhere (sync).
    void workspaceUpdated();
    void contactsUpdated();
    // Boards currently held in memory.
    [[nodiscard]] std::vector<QUuid> loadedBoards() const;
    // A name or colour change made in another app instance on this device.
    void applyInstanceProfile(const QString &name, int colorIndex);

  signals:
    void currentProjectChanged();
    void boardFilterChanged();
    void profileChanged();
    void contactsChanged();
    void workspaceChanged();
    // A board's contents changed, here or from a peer (C++ only, for sync).
    void boardChanged(const QUuid &boardId);
    void errorOccurred(const QString &message);
    void customColorsChanged();

  private:
    // Runs action, reporting a thrown error through errorOccurred; returns success.
    bool attempt(const std::function<void()> &action);
    [[nodiscard]] QVariantList people(const AccessState &state);
    [[nodiscard]] QString nameOf(const QByteArray &key, const AccessState &state) const;
    void closeRemovedBoards();
    // Adds someone to your known people unless they are already there.
    void ensureContact(const QString &keyHex, const QString &name);
    void refreshProjects();
    void refreshBoards();
    void refreshContacts();
    void ensureValidSelection();

    QString directory_;
    int workspaceRevision_{0};
    IdentityStore identity_;
    Workspace workspace_;
    QString currentProject_;
    QString boardFilter_;
    std::map<QUuid, BoardDocument *> documents_;
    std::map<QUuid, quint64> savedRevisions_;
    DraftListener draftListener_;
    QTimer *saveTimer_;
    RowModel *projects_;
    RowModel *boards_;
    RowModel *contacts_;
};

} // namespace peerchalk
