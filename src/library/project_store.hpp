#pragma once

#include "library/codes.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUuid>

#include <vector>

namespace peerchalk {

struct BoardInfo {
    QUuid id;
    QString name;
    QDateTime created;
    QDateTime edited;
};

struct ProjectInfo {
    QUuid id;
    QString name;
    QByteArray capability; // the secret that lets members discover each other
    int colorIndex{0};
    std::vector<BoardInfo> boards;
};

// This device's projects and their boards, kept in `directory`: the list in
// projects.json and each board's Yrs state in boards/<id>.yrs. projects.json holds
// project capabilities, so it is readable only by the owner. Failures to read or write
// throw std::runtime_error naming the file; invalid arguments (unknown ids, empty
// names) throw std::invalid_argument. Every change is saved before returning.
class ProjectStore {
  public:
    // Loads the list, or starts empty when there is none yet.
    explicit ProjectStore(QString directory);

    [[nodiscard]] const std::vector<ProjectInfo> &projects() const { return projects_; }
    [[nodiscard]] const ProjectInfo &project(const QUuid &id) const;
    [[nodiscard]] const BoardInfo &board(const QUuid &boardId) const;

    const ProjectInfo &createProject(const QString &name);
    // Adds the invited project; throws std::invalid_argument if it is already here.
    const ProjectInfo &joinProject(const Invite &invite);
    void renameProject(const QUuid &id, const QString &name);
    // Removes the project and its board files.
    void removeProject(const QUuid &id);

    const BoardInfo &createBoard(const QUuid &projectId, const QString &name);
    void renameBoard(const QUuid &boardId, const QString &name);
    void removeBoard(const QUuid &boardId);
    // The saved Yrs state of a board; empty when it has never been saved.
    [[nodiscard]] QByteArray boardState(const QUuid &boardId) const;
    // Saves a board's state and marks it edited now.
    void saveBoardState(const QUuid &boardId, const QByteArray &state);

  private:
    [[nodiscard]] ProjectInfo &findProject(const QUuid &id);
    [[nodiscard]] BoardInfo &findBoard(const QUuid &boardId);
    [[nodiscard]] QString boardPath(const QUuid &boardId) const;
    void load();
    void save() const;

    QString directory_;
    std::vector<ProjectInfo> projects_;
};

} // namespace peerchalk
