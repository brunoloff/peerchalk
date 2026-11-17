#pragma once

#include "library/identity_store.hpp"
#include "library/project_store.hpp"
#include "ui/row_model.hpp"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QQmlEngine;
class QJSEngine;

namespace peerchalk {

// The app's projects, boards and identity for QML. Data lives in the folder named by
// the PEERCHALK_DATA_DIR environment variable, or else the platform's app data folder.
// Operations that fail emit errorOccurred with a message for the user and return an
// empty value instead of throwing into QML.
class Library : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(peerchalk::RowModel *projects READ projects CONSTANT)
    Q_PROPERTY(peerchalk::RowModel *boards READ boards CONSTANT)
    Q_PROPERTY(peerchalk::RowModel *contacts READ contacts CONSTANT)
    Q_PROPERTY(QString currentProjectId READ currentProjectId WRITE setCurrentProjectId NOTIFY
                   currentProjectChanged)
    Q_PROPERTY(QString currentProjectName READ currentProjectName NOTIFY currentProjectChanged)
    // Only boards whose name contains this text (ignoring case) are listed.
    Q_PROPERTY(QString boardFilter READ boardFilter WRITE setBoardFilter NOTIFY boardFilterChanged)
    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName NOTIFY profileChanged)
    Q_PROPERTY(int colorIndex READ colorIndex WRITE setColorIndex NOTIFY profileChanged)
    Q_PROPERTY(QString fingerprint READ fingerprint CONSTANT)
    Q_PROPERTY(QString contactCode READ contactCode NOTIFY profileChanged)

  public:
    // Throws std::runtime_error when the stored data cannot be read.
    explicit Library(const QString &directory, QObject *parent = nullptr);
    static Library *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    [[nodiscard]] RowModel *projects() const { return projects_; }
    [[nodiscard]] RowModel *boards() const { return boards_; }
    [[nodiscard]] RowModel *contacts() const { return contacts_; }
    [[nodiscard]] QString currentProjectId() const;
    void setCurrentProjectId(const QString &id);
    [[nodiscard]] QString currentProjectName() const;
    [[nodiscard]] QString boardFilter() const { return boardFilter_; }
    void setBoardFilter(const QString &filter);
    [[nodiscard]] QString displayName() const { return identity_.displayName(); }
    void setDisplayName(const QString &name);
    [[nodiscard]] int colorIndex() const { return identity_.colorIndex(); }
    void setColorIndex(int index);
    [[nodiscard]] QString fingerprint() const;
    [[nodiscard]] QString contactCode() const;

    Q_INVOKABLE QString createProject(const QString &name);
    // Joins the project in an invite code; returns its id, or empty on failure.
    Q_INVOKABLE QString joinProject(const QString &code);
    Q_INVOKABLE QString projectInvite(const QString &projectId);
    Q_INVOKABLE void renameProject(const QString &projectId, const QString &name);
    Q_INVOKABLE void removeProject(const QString &projectId);

    // Boards are created in the current project.
    Q_INVOKABLE QString createBoard(const QString &name);
    Q_INVOKABLE void renameBoard(const QString &boardId, const QString &name);
    Q_INVOKABLE void removeBoard(const QString &boardId);
    Q_INVOKABLE QString boardName(const QString &boardId);
    Q_INVOKABLE QByteArray boardState(const QString &boardId);
    Q_INVOKABLE void saveBoardState(const QString &boardId, const QByteArray &state);

    // Adds the person in a contact code; returns false on failure.
    Q_INVOKABLE bool addContact(const QString &code);
    // how: "none", "in-person", "call" or "shared".
    Q_INVOKABLE void setVerification(const QString &keyHex, const QString &how);
    Q_INVOKABLE void removeContact(const QString &keyHex);
    Q_INVOKABLE void copyToClipboard(const QString &text);

  signals:
    void currentProjectChanged();
    void boardFilterChanged();
    void profileChanged();
    void errorOccurred(const QString &message);

  private:
    // Runs action, reporting a thrown error through errorOccurred; returns success.
    bool attempt(const std::function<void()> &action);
    void refreshProjects();
    void refreshBoards();
    void refreshContacts();

    ProjectStore store_;
    IdentityStore identity_;
    QUuid currentProject_;
    QString boardFilter_;
    RowModel *projects_;
    RowModel *boards_;
    RowModel *contacts_;
};

} // namespace peerchalk
