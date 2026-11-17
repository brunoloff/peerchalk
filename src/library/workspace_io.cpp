// Workspace persistence, changes from peers and personal sync.
#include "library/files.hpp"
#include "library/workspace.hpp"

#include <QDir>
#include <QJsonArray>

#include <algorithm>
#include <set>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr int formatVersion = 3;

QString base64(const QByteArray &bytes) { return QString::fromLatin1(bytes.toBase64()); }

QByteArray fromBase64(const QJsonValue &value) {
    return QByteArray::fromBase64(value.toString().toLatin1());
}

QJsonArray opsJson(const std::vector<SignedOp> &ops) {
    QJsonArray array;
    for (const SignedOp &op : ops)
        array.append(
            QJsonObject{{"payload", base64(op.payload)}, {"signature", base64(op.signature)}});
    return array;
}

std::vector<SignedOp> opsFromJson(const QJsonValue &value) {
    std::vector<SignedOp> ops;
    for (const QJsonValue &entry : value.toArray()) {
        std::optional<SignedOp> op =
            verifyOp(fromBase64(entry["payload"]), fromBase64(entry["signature"]));
        if (!op)
            throw std::runtime_error("a stored access change does not verify");
        ops.push_back(*std::move(op));
    }
    return ops;
}

QUuid requireId(const QJsonObject &object) {
    const QUuid id = QUuid::fromString(object["id"].toString());
    if (id.isNull())
        throw std::runtime_error("an entry has no valid id");
    return id;
}

QByteArray optionalKey(const QJsonObject &object, const char *name) {
    const QByteArray key = fromBase64(object[QString::fromLatin1(name)]);
    if (!key.isEmpty() && key.size() != 32)
        throw std::runtime_error(std::string("an entry has a damaged ") + name);
    return key;
}

QJsonObject toJson(const ProjectEntry &entry) {
    return {{"id", entry.id.toString(QUuid::WithoutBraces)},
            {"capability", base64(entry.capability)},
            {"creator", base64(entry.creator)},
            {"inviteName", entry.inviteName},
            {"color", entry.colorIndex},
            {"ops", opsJson(entry.ops)}};
}

QJsonObject toJson(const BoardEntry &entry) {
    return {{"id", entry.id.toString(QUuid::WithoutBraces)},
            {"capability", base64(entry.capability)},
            {"creator", base64(entry.creator)},
            {"inviteName", entry.inviteName},
            {"edited", entry.edited.toString(Qt::ISODateWithMs)},
            {"ops", opsJson(entry.ops)}};
}

ProjectEntry projectFromJson(const QJsonObject &object) {
    return {requireId(object),
            base64Field(object, QStringLiteral("capability"), 32),
            base64Field(object, QStringLiteral("creator"), 32),
            object["inviteName"].toString(),
            object["color"].toInt(),
            opsFromJson(object["ops"]),
            {}};
}

BoardEntry boardFromJson(const QJsonObject &object) {
    return {requireId(object),
            optionalKey(object, "capability"),
            optionalKey(object, "creator"),
            object["inviteName"].toString(),
            QDateTime::fromString(object["edited"].toString(), Qt::ISODateWithMs),
            opsFromJson(object["ops"]),
            {}};
}

// Adds the changes not yet in `ops`; returns whether any were new.
bool addMissing(std::vector<SignedOp> &ops, const std::vector<SignedOp> &incoming) {
    bool added = false;
    for (const SignedOp &op : incoming) {
        if (std::ranges::find(ops, op.hash, &SignedOp::hash) == ops.end()) {
            ops.push_back(op);
            added = true;
        }
    }
    return added;
}

std::vector<SignedOp> forResource(const std::vector<SignedOp> &ops, ResourceKind kind,
                                  const QUuid &id) {
    std::vector<SignedOp> result;
    for (const SignedOp &op : ops)
        if (op.op.kind == kind && op.op.resource == id)
            result.push_back(op);
    return result;
}

} // namespace

void Workspace::load() {
    const QString path = QDir(directory_).filePath(QStringLiteral("workspace.json"));
    const std::optional<int> version = jsonFileVersion(path);
    if (!version || *version < formatVersion) {
        if (migrate())
            save();
        return;
    }
    const std::optional<QJsonObject> root = readJsonObject(path, formatVersion);
    try {
        for (const QJsonValue &value : (*root)["projects"].toArray())
            projects_.push_back(projectFromJson(value.toObject()));
        for (const QJsonValue &value : (*root)["boards"].toArray())
            boards_.push_back(boardFromJson(value.toObject()));
    } catch (const std::exception &error) {
        throw std::runtime_error(path.toStdString() + ": " + error.what());
    }
    recompute();
}

void Workspace::save() const {
    writeJsonObject(QDir(directory_).filePath(QStringLiteral("workspace.json")), exportPersonal(),
                    formatVersion, true);
}

bool Workspace::addOp(const SignedOp &op) {
    std::vector<SignedOp> *ops = nullptr;
    if (op.op.kind == ResourceKind::Project && hasProject(op.op.resource))
        ops = &findProject(op.op.resource).ops;
    else if (op.op.kind == ResourceKind::Board && hasBoard(op.op.resource))
        ops = &findBoard(op.op.resource).ops;
    return ops != nullptr && addMissing(*ops, {op});
}

// A board you know only through a project is anchored by the creator that the project's
// SetBoard change names. Each candidate creator from projects you may access is tried,
// and the board is kept only if it then turns out accessible, so a member's forged
// changes cannot take over a real board.
bool Workspace::learnBoard(const QUuid &boardId, const std::vector<SignedOp> &boardOps) {
    std::set<QByteArray> candidates;
    for (const ProjectEntry &project : projects_) {
        if (!mayAccessProject(user_, project.id))
            continue;
        for (const SignedOp &op : project.ops)
            if (op.op.action == AccessOp::Action::SetBoard && op.op.board == boardId &&
                op.op.included)
                candidates.insert(op.op.user);
    }
    for (const QByteArray &creator : candidates) {
        boards_.push_back(
            {boardId, {}, creator, {}, QDateTime::currentDateTimeUtc(), boardOps, {}});
        recompute();
        if (mayAccessBoard(user_, boardId))
            return true;
        boards_.pop_back();
    }
    recompute();
    return false;
}

bool Workspace::acceptOps(const std::vector<std::pair<QByteArray, QByteArray>> &signedOps) {
    std::vector<SignedOp> verified;
    for (const auto &[payload, signature] : signedOps)
        if (std::optional<SignedOp> op = verifyOp(payload, signature))
            verified.push_back(*std::move(op));
    bool changed = false;
    for (const SignedOp &op : verified)
        if (op.op.kind == ResourceKind::Project)
            changed |= addOp(op);
    recompute();
    std::set<QUuid> unknownBoards;
    for (const SignedOp &op : verified) {
        if (op.op.kind != ResourceKind::Board)
            continue;
        if (hasBoard(op.op.resource))
            changed |= addOp(op);
        else
            unknownBoards.insert(op.op.resource);
    }
    for (const QUuid &id : unknownBoards)
        changed |= learnBoard(id, forResource(verified, ResourceKind::Board, id));
    recompute();
    const auto unreachable = [this](const BoardEntry &board) {
        return board.capability.isEmpty() && board.creator != user_ &&
               !mayAccessBoard(user_, board.id);
    };
    changed |= std::erase_if(boards_, unreachable) > 0;
    if (changed)
        save();
    return changed;
}

std::vector<SignedOp> Workspace::opsForProject(const QUuid &projectId) const {
    if (!hasProject(projectId))
        return {};
    std::vector<SignedOp> result = project(projectId).ops;
    for (const QUuid &id : project(projectId).state.boards)
        if (hasBoard(id))
            std::ranges::copy(board(id).ops, std::back_inserter(result));
    return result;
}

std::vector<SignedOp> Workspace::opsForBoard(const QUuid &boardId) const {
    return hasBoard(boardId) ? board(boardId).ops : std::vector<SignedOp>{};
}

std::vector<SignedOp> Workspace::allOps() const {
    std::vector<SignedOp> result;
    for (const ProjectEntry &project : projects_)
        std::ranges::copy(project.ops, std::back_inserter(result));
    for (const BoardEntry &board : boards_)
        std::ranges::copy(board.ops, std::back_inserter(result));
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

// Your own devices are trusted with capabilities; their changes still need valid
// signatures and only count if replay accepts them.
bool Workspace::mergePersonal(const QJsonObject &personal) {
    bool changed = false;
    for (const QJsonValue &value : personal["projects"].toArray()) {
        const ProjectEntry incoming = projectFromJson(value.toObject());
        if (!hasProject(incoming.id)) {
            projects_.push_back(incoming);
            changed = true;
        } else if (findProject(incoming.id).creator == incoming.creator) {
            changed |= addMissing(findProject(incoming.id).ops, incoming.ops);
        }
    }
    for (const QJsonValue &value : personal["boards"].toArray()) {
        const BoardEntry incoming = boardFromJson(value.toObject());
        if (!hasBoard(incoming.id)) {
            boards_.push_back(incoming);
            changed = true;
            continue;
        }
        BoardEntry &entry = findBoard(incoming.id);
        if (entry.creator != incoming.creator)
            continue;
        if (entry.capability.isEmpty() && !incoming.capability.isEmpty()) {
            entry.capability = incoming.capability;
            changed = true;
        }
        changed |= addMissing(entry.ops, incoming.ops);
    }
    if (changed) {
        recompute();
        save();
    }
    return changed;
}

} // namespace peerchalk
