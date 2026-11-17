// Workspace persistence, migration, manifests from peers and personal sync.
#include "library/files.hpp"
#include "library/secrets.hpp"
#include "library/workspace.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>

#include <algorithm>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr int formatVersion = 2;

QString base64(const QByteArray &bytes) { return QString::fromLatin1(bytes.toBase64()); }

QByteArray fromBase64(const QJsonValue &value) {
    return QByteArray::fromBase64(value.toString().toLatin1());
}

QJsonValue manifestJson(const std::optional<SignedManifest> &manifest) {
    if (!manifest)
        return QJsonValue::Null;
    return QJsonObject{{"payload", base64(manifest->payload)},
                       {"signature", base64(manifest->signature)}};
}

// The stored manifest, checked against the entry's admin; throws when it does not verify.
std::optional<SignedManifest> manifestFromJson(const QJsonValue &value, ResourceKind kind,
                                               const QUuid &id, const QByteArray &admin) {
    if (value.isNull() || value.isUndefined())
        return std::nullopt;
    std::optional<SignedManifest> manifest =
        verifyManifest(fromBase64(value[QStringLiteral("payload")]),
                       fromBase64(value[QStringLiteral("signature")]), kind, id, admin);
    if (!manifest)
        throw std::runtime_error("a stored manifest does not verify");
    return manifest;
}

QJsonObject toJson(const ProjectEntry &entry) {
    return {{"id", entry.id.toString(QUuid::WithoutBraces)},
            {"capability", base64(entry.capability)},
            {"admin", base64(entry.admin)},
            {"inviteName", entry.inviteName},
            {"color", entry.colorIndex},
            {"manifest", manifestJson(entry.manifest)}};
}

QJsonObject toJson(const BoardEntry &entry) {
    return {{"id", entry.id.toString(QUuid::WithoutBraces)},
            {"capability", base64(entry.capability)},
            {"admin", base64(entry.admin)},
            {"inviteName", entry.inviteName},
            {"edited", entry.edited.toString(Qt::ISODateWithMs)},
            {"manifest", manifestJson(entry.manifest)}};
}

QUuid requireId(const QJsonObject &object) {
    const QUuid id = QUuid::fromString(object[QStringLiteral("id")].toString());
    if (id.isNull())
        throw std::runtime_error("an entry has no valid id");
    return id;
}

ProjectEntry projectFromJson(const QJsonObject &object) {
    ProjectEntry entry{requireId(object),
                       base64Field(object, QStringLiteral("capability"), 32),
                       base64Field(object, QStringLiteral("admin"), 32),
                       object[QStringLiteral("inviteName")].toString(),
                       object[QStringLiteral("color")].toInt(),
                       std::nullopt};
    entry.manifest = manifestFromJson(object[QStringLiteral("manifest")], ResourceKind::Project,
                                      entry.id, entry.admin);
    return entry;
}

BoardEntry boardFromJson(const QJsonObject &object) {
    BoardEntry entry{
        requireId(object),
        fromBase64(object[QStringLiteral("capability")]),
        base64Field(object, QStringLiteral("admin"), 32),
        object[QStringLiteral("inviteName")].toString(),
        std::nullopt,
        QDateTime::fromString(object[QStringLiteral("edited")].toString(), Qt::ISODateWithMs)};
    if (!entry.capability.isEmpty() && entry.capability.size() != 32)
        throw std::runtime_error("a board has a damaged capability");
    entry.manifest = manifestFromJson(object[QStringLiteral("manifest")], ResourceKind::Board,
                                      entry.id, entry.admin);
    return entry;
}

bool isNewer(const std::optional<SignedManifest> &current, const SignedManifest &candidate) {
    return !current || candidate.manifest.version > current->manifest.version;
}

} // namespace

void Workspace::load() {
    const QString path = QDir(directory_).filePath(QStringLiteral("workspace.json"));
    const std::optional<QJsonObject> root = readJsonObject(path, formatVersion);
    if (!root) {
        migrateProjectsFile();
        return;
    }
    try {
        for (const QJsonValue &value : (*root)[QStringLiteral("projects")].toArray())
            projects_.push_back(projectFromJson(value.toObject()));
        for (const QJsonValue &value : (*root)[QStringLiteral("boards")].toArray())
            boards_.push_back(boardFromJson(value.toObject()));
    } catch (const std::exception &error) {
        throw std::runtime_error(path.toStdString() + ": " + error.what());
    }
}

void Workspace::save() const {
    writeJsonObject(QDir(directory_).filePath(QStringLiteral("workspace.json")), exportPersonal(),
                    formatVersion, true);
}

// Earlier versions kept projects with nested boards in projects.json, all created here;
// they become projects and boards administered by this user.
void Workspace::migrateProjectsFile() {
    const QString path = QDir(directory_).filePath(QStringLiteral("projects.json"));
    const std::optional<QJsonObject> old = readJsonObject(path, 1);
    if (!old)
        return;
    for (const QJsonValue &value : (*old)[QStringLiteral("projects")].toArray()) {
        const QJsonObject project = value.toObject();
        const QUuid projectId = requireId(project);
        const QString name = project[QStringLiteral("name")].toString();
        projects_.push_back({projectId, base64Field(project, QStringLiteral("capability"), 32),
                             user_, name, project[QStringLiteral("color")].toInt(), std::nullopt});
        projects_.back().manifest = signManifest(
            {ResourceKind::Project, projectId, name, user_, 1, {}, {}, false}, signer_);
        for (const QJsonValue &boardValue : project[QStringLiteral("boards")].toArray()) {
            const QJsonObject board = boardValue.toObject();
            const QUuid boardId = requireId(board);
            const QString boardName = board[QStringLiteral("name")].toString();
            boards_.push_back({boardId, newCapability(), user_, boardName, std::nullopt,
                               QDateTime::fromString(board[QStringLiteral("edited")].toString(),
                                                     Qt::ISODateWithMs)});
            boards_.back().manifest = signManifest(
                {ResourceKind::Board, boardId, boardName, user_, 1, {}, {projectId}, false},
                signer_);
        }
    }
    save();
    QFile::rename(path, QDir(directory_).filePath(QStringLiteral("projects.v1-migrated.json")));
}

bool Workspace::acceptManifest(const QByteArray &payload, const QByteArray &signature) {
    const std::optional<SignedManifest> received = verifyAnyManifest(payload, signature);
    if (!received)
        return false;
    const Manifest &manifest = received->manifest;
    if (manifest.kind == ResourceKind::Board)
        return acceptBoardManifest(*received);
    if (!hasProject(manifest.id))
        return false;
    ProjectEntry &entry = findProject(manifest.id);
    if (entry.admin != manifest.admin || !isNewer(entry.manifest, *received))
        return false;
    entry.manifest = received;
    save();
    return true;
}

bool Workspace::acceptBoardManifest(const SignedManifest &received) {
    const Manifest &manifest = received.manifest;
    if (hasBoard(manifest.id)) {
        BoardEntry &entry = findBoard(manifest.id);
        if (entry.admin != manifest.admin || !isNewer(entry.manifest, received))
            return false;
        entry.manifest = received;
        save();
        return true;
    }
    const bool vouched = std::ranges::any_of(manifest.projects, [&](const QUuid &projectId) {
        return mayAccessProject(user_, projectId) &&
               project(projectId).manifest->manifest.includes(manifest.admin);
    });
    if (!vouched || manifest.deleted)
        return false;
    boards_.push_back({manifest.id,
                       {},
                       manifest.admin,
                       manifest.name,
                       received,
                       QDateTime::currentDateTimeUtc()});
    save();
    return true;
}

std::vector<SignedManifest> Workspace::manifestsForProject(const QUuid &projectId) const {
    std::vector<SignedManifest> result;
    if (!hasProject(projectId) || !project(projectId).manifest)
        return result;
    result.push_back(*project(projectId).manifest);
    for (const BoardEntry &board : boards_) {
        if (board.manifest && std::ranges::find(board.manifest->manifest.projects, projectId) !=
                                  board.manifest->manifest.projects.end())
            result.push_back(*board.manifest);
    }
    return result;
}

std::vector<SignedManifest> Workspace::manifestsForBoard(const QUuid &boardId) const {
    if (!hasBoard(boardId) || !board(boardId).manifest)
        return {};
    return {*board(boardId).manifest};
}

std::vector<SignedManifest> Workspace::allManifests() const {
    std::vector<SignedManifest> result;
    for (const ProjectEntry &project : projects_)
        if (project.manifest)
            result.push_back(*project.manifest);
    for (const BoardEntry &board : boards_)
        if (board.manifest)
            result.push_back(*board.manifest);
    return result;
}

QJsonObject Workspace::exportPersonal() const {
    QJsonArray projects;
    for (const ProjectEntry &entry : projects_)
        projects.append(toJson(entry));
    QJsonArray boards;
    for (const BoardEntry &entry : boards_)
        boards.append(toJson(entry));
    return {{"projects", projects}, {"boards", boards}};
}

// Your own devices are trusted with capabilities, but their manifests must still verify.
bool Workspace::mergePersonal(const QJsonObject &personal) {
    bool changed = false;
    for (const QJsonValue &value : personal[QStringLiteral("projects")].toArray()) {
        const ProjectEntry incoming = projectFromJson(value.toObject());
        if (!hasProject(incoming.id)) {
            projects_.push_back(incoming);
            changed = true;
        } else if (incoming.manifest && findProject(incoming.id).admin == incoming.admin &&
                   isNewer(findProject(incoming.id).manifest, *incoming.manifest)) {
            findProject(incoming.id).manifest = incoming.manifest;
            changed = true;
        }
    }
    for (const QJsonValue &value : personal[QStringLiteral("boards")].toArray()) {
        const BoardEntry incoming = boardFromJson(value.toObject());
        if (!hasBoard(incoming.id)) {
            boards_.push_back(incoming);
            changed = true;
            continue;
        }
        BoardEntry &entry = findBoard(incoming.id);
        if (entry.admin != incoming.admin)
            continue;
        if (entry.capability.isEmpty() && !incoming.capability.isEmpty()) {
            entry.capability = incoming.capability;
            changed = true;
        }
        if (incoming.manifest && isNewer(entry.manifest, *incoming.manifest)) {
            entry.manifest = incoming.manifest;
            changed = true;
        }
    }
    if (changed)
        save();
    return changed;
}

} // namespace peerchalk
