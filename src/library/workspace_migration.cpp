// Workspace: converting files from earlier versions into signed access changes.
#include "library/files.hpp"
#include "library/secrets.hpp"
#include "library/workspace.hpp"
#include "library/workspace_support.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace peerchalk {

using namespace workspace_detail;
using Action = AccessOp::Action;

namespace {

// One project or board as earlier versions stored it.
struct Legacy {
    ResourceKind kind{ResourceKind::Project};
    QUuid id;
    QString name;
    QByteArray capability;
    QByteArray admin;
    std::vector<std::pair<QByteArray, QString>> members;
    std::vector<QUuid> projects; // boards
    bool deleted{false};
    int color{0};
    QDateTime edited;
};

QByteArray fromBase64(const QJsonValue &value) {
    return QByteArray::fromBase64(value.toString().toLatin1());
}

// Version 2: each entry carried its admin's last signed manifest.
Legacy fromVersion2(const QJsonObject &entry, ResourceKind kind) {
    Legacy legacy{kind,
                  QUuid::fromString(entry["id"].toString()),
                  entry["inviteName"].toString(),
                  fromBase64(entry["capability"]),
                  fromBase64(entry["admin"]),
                  {},
                  {},
                  false,
                  entry["color"].toInt(),
                  QDateTime::fromString(entry["edited"].toString(), Qt::ISODateWithMs)};
    const QJsonObject manifest =
        QJsonDocument::fromJson(fromBase64(entry["manifest"]["payload"])).object();
    if (manifest.isEmpty())
        return legacy;
    legacy.name = manifest["name"].toString();
    legacy.deleted = manifest["deleted"].toBool();
    for (const QJsonValue &member : manifest["members"].toArray())
        legacy.members.push_back({fromBase64(member["key"]), member["name"].toString()});
    for (const QJsonValue &project : manifest["projects"].toArray())
        legacy.projects.push_back(QUuid::fromString(project.toString()));
    return legacy;
}

std::vector<Legacy> readVersion2(const QJsonObject &root) {
    std::vector<Legacy> entries;
    for (const QJsonValue &value : root["projects"].toArray())
        entries.push_back(fromVersion2(value.toObject(), ResourceKind::Project));
    for (const QJsonValue &value : root["boards"].toArray())
        entries.push_back(fromVersion2(value.toObject(), ResourceKind::Board));
    return entries;
}

// Version 1: projects.json with nested boards, all created on this device.
std::vector<Legacy> readVersion1(const QJsonObject &root, const QByteArray &user) {
    std::vector<Legacy> entries;
    for (const QJsonValue &value : root["projects"].toArray()) {
        const QUuid projectId = QUuid::fromString(value["id"].toString());
        entries.push_back({ResourceKind::Project,
                           projectId,
                           value["name"].toString(),
                           fromBase64(value["capability"]),
                           user,
                           {},
                           {},
                           false,
                           value["color"].toInt(),
                           {}});
        for (const QJsonValue &board : value["boards"].toArray())
            entries.push_back(
                {ResourceKind::Board,
                 QUuid::fromString(board["id"].toString()),
                 board["name"].toString(),
                 {},
                 user,
                 {},
                 {projectId},
                 false,
                 0,
                 QDateTime::fromString(board["edited"].toString(), Qt::ISODateWithMs)});
    }
    return entries;
}

} // namespace

// Entries you administered are re-created as signed changes; others keep their invite
// details and receive their changes from peers again.
bool Workspace::migrate() {
    const QDir dir(directory_);
    std::vector<Legacy> entries;
    QString oldFile;
    if (const auto v2 = readJsonObject(dir.filePath(QStringLiteral("workspace.json")), 2)) {
        entries = readVersion2(*v2);
        oldFile = QStringLiteral("workspace.json");
    } else if (const auto v1 = readJsonObject(dir.filePath(QStringLiteral("projects.json")), 1)) {
        entries = readVersion1(*v1, user_);
        oldFile = QStringLiteral("projects.json");
    } else {
        return false;
    }
    // Moved aside first: recording the new changes saves workspace.json.
    if (!QFile::rename(dir.filePath(oldFile), dir.filePath(oldFile + QStringLiteral(".migrated"))))
        throw std::runtime_error(dir.filePath(oldFile).toStdString() + ": cannot move it aside");
    for (const Legacy &legacy : entries) {
        const QByteArray capability = legacy.capability.size() == 32 ? legacy.capability
                                      : legacy.admin == user_        ? newCapability()
                                                                     : QByteArray();
        if (legacy.kind == ResourceKind::Project)
            projects_.push_back(
                {legacy.id, capability, legacy.admin, legacy.name, legacy.color, {}, {}});
        else
            boards_.push_back(
                {legacy.id,
                 capability,
                 legacy.admin,
                 legacy.name,
                 legacy.edited.isValid() ? legacy.edited : QDateTime::currentDateTimeUtc(),
                 {},
                 {}});
    }
    for (const Legacy &legacy : entries)
        if (legacy.admin == user_)
            recreate(legacy.kind, legacy.id, legacy.name, legacy.members, legacy.deleted);
    for (const Legacy &legacy : entries)
        if (legacy.admin == user_ && legacy.kind == ResourceKind::Board)
            for (const QUuid &project : legacy.projects)
                if (hasProject(project) && projectRole(project, user_) >= Role::Owner)
                    setBoardInProject(legacy.id, project, true);
    recompute();
    return true;
}

void Workspace::recreate(ResourceKind kind, const QUuid &id, const QString &name,
                         const std::vector<std::pair<QByteArray, QString>> &members, bool deleted) {
    std::vector<SignedOp> &ops =
        kind == ResourceKind::Project ? findProject(id).ops : findBoard(id).ops;
    AccessOp create = change(kind, id, Action::Create);
    create.name = requireName(name);
    record(ops, 0, create);
    quint64 clock = 1;
    for (const auto &[key, memberName] : members) {
        AccessOp member = change(kind, id, Action::SetRole);
        member.user = key;
        member.name = memberName;
        member.role = Role::Member;
        record(ops, clock++, member);
    }
    if (deleted)
        record(ops, clock, change(kind, id, Action::Delete));
}

} // namespace peerchalk
