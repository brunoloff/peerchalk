#include "library/project_store.hpp"

#include "library/files.hpp"

#include <p2pdc/discovery_key.h>

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <stdexcept>
#include <variant>

namespace peerchalk {
namespace {

constexpr int formatVersion = 1;
constexpr int projectColorCount = 6;

QString requireName(const QString &name) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        throw std::invalid_argument("Names cannot be empty");
    return trimmed;
}

QByteArray newCapability() {
    const auto result = p2pdc::generate_discovery_capability();
    if (const auto *capability = std::get_if<p2pdc::DiscoveryCapability>(&result))
        return {reinterpret_cast<const char *>(capability->data()),
                static_cast<qsizetype>(capability->size())};
    throw std::runtime_error("Could not generate a project secret");
}

QJsonObject toJson(const BoardInfo &board) {
    return {{QStringLiteral("id"), board.id.toString(QUuid::WithoutBraces)},
            {QStringLiteral("name"), board.name},
            {QStringLiteral("created"), board.created.toString(Qt::ISODateWithMs)},
            {QStringLiteral("edited"), board.edited.toString(Qt::ISODateWithMs)}};
}

QJsonObject toJson(const ProjectInfo &project) {
    QJsonArray boards;
    for (const BoardInfo &board : project.boards)
        boards.append(toJson(board));
    return {{QStringLiteral("id"), project.id.toString(QUuid::WithoutBraces)},
            {QStringLiteral("name"), project.name},
            {QStringLiteral("capability"), QString::fromLatin1(project.capability.toBase64())},
            {QStringLiteral("color"), project.colorIndex},
            {QStringLiteral("boards"), boards}};
}

QUuid requireId(const QJsonObject &object) {
    const QUuid id = QUuid::fromString(object.value(QStringLiteral("id")).toString());
    if (id.isNull())
        throw std::runtime_error("an entry has no valid id");
    return id;
}

BoardInfo boardFromJson(const QJsonObject &object) {
    return {requireId(object), requireName(object.value(QStringLiteral("name")).toString()),
            QDateTime::fromString(object.value(QStringLiteral("created")).toString(),
                                  Qt::ISODateWithMs),
            QDateTime::fromString(object.value(QStringLiteral("edited")).toString(),
                                  Qt::ISODateWithMs)};
}

ProjectInfo projectFromJson(const QJsonObject &object) {
    ProjectInfo project{requireId(object),
                        requireName(object.value(QStringLiteral("name")).toString()),
                        base64Field(object, QStringLiteral("capability"), capabilitySize),
                        object.value(QStringLiteral("color")).toInt() % projectColorCount,
                        {}};
    for (const QJsonValue &board : object.value(QStringLiteral("boards")).toArray())
        project.boards.push_back(boardFromJson(board.toObject()));
    return project;
}

} // namespace

ProjectStore::ProjectStore(QString directory) : directory_(std::move(directory)) { load(); }

void ProjectStore::load() {
    const QString path = QDir(directory_).filePath(QStringLiteral("projects.json"));
    const std::optional<QJsonObject> root = readJsonObject(path, formatVersion);
    if (!root)
        return;
    try {
        for (const QJsonValue &project : root->value(QStringLiteral("projects")).toArray())
            projects_.push_back(projectFromJson(project.toObject()));
    } catch (const std::exception &error) {
        throw std::runtime_error(path.toStdString() + ": " + error.what());
    }
}

void ProjectStore::save() const {
    QJsonArray projects;
    for (const ProjectInfo &project : projects_)
        projects.append(toJson(project));
    writeJsonObject(QDir(directory_).filePath(QStringLiteral("projects.json")),
                    {{QStringLiteral("projects"), projects}}, formatVersion, true);
}

const ProjectInfo &ProjectStore::project(const QUuid &id) const {
    const auto it = std::ranges::find(projects_, id, &ProjectInfo::id);
    if (it == projects_.end())
        throw std::invalid_argument("Unknown project");
    return *it;
}

ProjectInfo &ProjectStore::findProject(const QUuid &id) {
    return const_cast<ProjectInfo &>(std::as_const(*this).project(id));
}

const BoardInfo &ProjectStore::board(const QUuid &boardId) const {
    for (const ProjectInfo &project : projects_) {
        const auto it = std::ranges::find(project.boards, boardId, &BoardInfo::id);
        if (it != project.boards.end())
            return *it;
    }
    throw std::invalid_argument("Unknown board");
}

BoardInfo &ProjectStore::findBoard(const QUuid &boardId) {
    return const_cast<BoardInfo &>(std::as_const(*this).board(boardId));
}

QString ProjectStore::boardPath(const QUuid &boardId) const {
    return QDir(directory_)
        .filePath(QStringLiteral("boards/%1.yrs").arg(boardId.toString(QUuid::WithoutBraces)));
}

const ProjectInfo &ProjectStore::createProject(const QString &name) {
    const int color = static_cast<int>(projects_.size()) % projectColorCount;
    projects_.push_back({QUuid::createUuid(), requireName(name), newCapability(), color, {}});
    save();
    return projects_.back();
}

const ProjectInfo &ProjectStore::joinProject(const Invite &invite) {
    if (std::ranges::find(projects_, invite.project, &ProjectInfo::id) != projects_.end())
        throw std::invalid_argument("You are already in this project");
    if (invite.capability.size() != capabilitySize)
        throw std::invalid_argument("The invite's secret has the wrong size");
    const int color = static_cast<int>(projects_.size()) % projectColorCount;
    projects_.push_back({invite.project, requireName(invite.name), invite.capability, color, {}});
    save();
    return projects_.back();
}

void ProjectStore::renameProject(const QUuid &id, const QString &name) {
    findProject(id).name = requireName(name);
    save();
}

void ProjectStore::removeProject(const QUuid &id) {
    const ProjectInfo &project = findProject(id);
    for (const BoardInfo &board : project.boards)
        removeFile(boardPath(board.id));
    std::erase_if(projects_, [&id](const ProjectInfo &p) { return p.id == id; });
    save();
}

const BoardInfo &ProjectStore::createBoard(const QUuid &projectId, const QString &name) {
    ProjectInfo &project = findProject(projectId);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    project.boards.push_back({QUuid::createUuid(), requireName(name), now, now});
    save();
    return project.boards.back();
}

void ProjectStore::renameBoard(const QUuid &boardId, const QString &name) {
    findBoard(boardId).name = requireName(name);
    save();
}

void ProjectStore::removeBoard(const QUuid &boardId) {
    static_cast<void>(board(boardId)); // throws for unknown boards
    removeFile(boardPath(boardId));
    for (ProjectInfo &project : projects_)
        std::erase_if(project.boards, [&boardId](const BoardInfo &b) { return b.id == boardId; });
    save();
}

QByteArray ProjectStore::boardState(const QUuid &boardId) const {
    static_cast<void>(board(boardId)); // throws for unknown boards
    return readFile(boardPath(boardId)).value_or(QByteArray());
}

void ProjectStore::saveBoardState(const QUuid &boardId, const QByteArray &state) {
    BoardInfo &board = findBoard(boardId);
    writeFile(boardPath(boardId), state, true);
    board.edited = QDateTime::currentDateTimeUtc();
    save();
}

} // namespace peerchalk
