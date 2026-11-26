#include "tools/board_tool.hpp"

#include "repository/board_writer.hpp"
#include "repository/encoding.hpp"

#include <QJsonArray>

#include <functional>
#include <map>
#include <memory>

namespace peerchalk::tools {
namespace {

using namespace repository;

const std::array<QString, 4> roleNames = {QStringLiteral("none"), QStringLiteral("member"),
                                          QStringLiteral("owner"), QStringLiteral("admin")};

QString roleName(Role role) { return roleNames.at(static_cast<std::size_t>(role)); }

QByteArray decoded(const QJsonValue &value, const QString &what) {
    const QByteArray::FromBase64Result result = QByteArray::fromBase64Encoding(
        value.toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!value.isString() || !result)
        throw UsageError(what.toStdString() + " is not base64");
    return result.decoded;
}

// {"files": {path: base64}, "access": [base64]}: one board's files and its history.
std::pair<BoardFiles, std::vector<QByteArray>> boardInput(const QJsonObject &object) {
    BoardFiles files;
    const QJsonObject given = object["files"].toObject();
    for (auto it = given.begin(); it != given.end(); ++it)
        files[it.key()] = decoded(it.value(), it.key());
    std::vector<QByteArray> access;
    for (const QJsonValue &entry : object["access"].toArray())
        access.push_back(decoded(entry, QStringLiteral("an access entry")));
    return {files, access};
}

LoadedBoard loadFiles(const BoardFiles &files, const std::vector<QByteArray> &access) {
    try {
        return loadBoard(files, access);
    } catch (const FileError &error) {
        throw RefusedError(error.what());
    }
}

LoadedBoard loadInput(const QJsonObject &object) {
    const auto [files, access] = boardInput(object);
    return loadFiles(files, access);
}

// The board's merged document (a Board neither copies nor moves).
std::unique_ptr<Board> documentOf(const LoadedBoard &board) {
    auto document = std::make_unique<Board>();
    document->apply(byteView(board.state));
    return document;
}

QJsonArray warningsJson(const LoadedBoard &board) {
    QJsonArray warnings;
    for (const LoadWarning &warning : board.warnings)
        warnings.append(QJsonObject{{"path", warning.path}, {"reason", warning.reason}});
    return warnings;
}

QJsonObject summary(const LoadedBoard &board, const QByteArray &me) {
    const std::unique_ptr<Board> document = documentOf(board);
    QJsonArray people;
    for (const auto &[user, role] : board.access.roles) {
        const auto name = board.access.names.find(user);
        people.append(
            QJsonObject{{"user", toHex(user)},
                        {"name", name == board.access.names.end() ? QString() : name->second},
                        {"role", roleName(role)}});
    }
    return {{"board", board.info.board.toString(QUuid::WithoutBraces)},
            {"name", board.name},
            {"creator", toHex(board.info.creator)},
            {"role", roleName(board.access.role(me))},
            {"people", people},
            {"epoch", board.latestEpoch()},
            {"snapshots", static_cast<int>(board.snapshots.size())},
            {"changes", static_cast<int>(board.changes.size())},
            {"strokes", static_cast<int>(document->strokes().size())},
            {"warnings", warningsJson(board)}};
}

QJsonObject filesJson(const FileChanges &changes, const QString &prefix) {
    QJsonObject files;
    for (const auto &[path, bytes] : changes)
        files[prefix + path] = bytes ? QJsonValue(base64(*bytes)) : QJsonValue();
    return files;
}

QString requireOption(const Invocation &call, const QString &name) {
    const QString value = call.options.value(name).trimmed();
    if (value.isEmpty())
        throw UsageError("--" + name.toStdString() + " is required");
    return value;
}

QJsonObject identity(const Invocation &, UserIdentity &id) {
    const DeviceCertificate certificate = id.deviceCertificate();
    return {{"userKey", toHex(id.userKey())},
            {"deviceKey", toHex(id.deviceKey())},
            {"deviceName", id.deviceName()},
            {"certificate", QJsonObject{{"payload", base64(certificate.payload)},
                                        {"signature", base64(certificate.signature)}}}};
}

QJsonObject importIdentity(const Invocation &call, UserIdentity &id) {
    if (call.arguments.size() != 1)
        throw UsageError("identity-import takes one transfer code");
    try {
        const QString name = id.adopt(call.arguments.front());
        return {{"userKey", toHex(id.userKey())}, {"name", name}};
    } catch (const std::invalid_argument &error) {
        throw UsageError(error.what());
    }
}

QJsonObject deviceSecrets(const Invocation &call, UserIdentity &id) {
    QJsonObject result{{"transferCode", id.transferCode(call.options.value("name"))},
                       {"deviceName", id.deviceName()}};
    id.withDeviceSecrets([&](std::span<const std::byte> seed, std::span<const std::byte> secret) {
        result["deviceSeed"] = base64(toByteArray(seed));
        result["recipientSecret"] = base64(toByteArray(secret));
    });
    return result;
}

QJsonObject person(const Invocation &call, UserIdentity &id) {
    const Person me{id.userKey(),
                    requireOption(call, "name"),
                    call.options.value("email"),
                    {id.deviceCertificate()}};
    return {{"path", "people/" + toHex(id.userKey()) + ".json"},
            {"data", base64(encodePerson(me, id.signer()))}};
}

QJsonObject create(const Invocation &call, UserIdentity &id) {
    try {
        const NewBoard board = createBoard(requireOption(call, "name"),
                                           call.options.value("display-name"), authorOf(id));
        const QString id36 = board.id.toString(QUuid::WithoutBraces);
        return {{"board", id36}, {"files", filesJson(board.files, "boards/" + id36 + "/")}};
    } catch (const std::invalid_argument &error) {
        throw UsageError(error.what());
    }
}

QJsonObject list(const Invocation &call, UserIdentity &id) {
    QJsonArray boards;
    const QJsonObject given = call.input["boards"].toObject();
    for (auto it = given.begin(); it != given.end(); ++it) {
        try {
            boards.append(summary(loadInput(it.value().toObject()), id.userKey()));
        } catch (const RefusedError &error) {
            boards.append(QJsonObject{{"board", it.key()}, {"error", error.what()}});
        }
    }
    return {{"boards", boards}};
}

QJsonObject open(const Invocation &call, UserIdentity &id) {
    const LoadedBoard board = loadInput(call.input);
    QJsonObject result = summary(board, id.userKey());
    const std::optional<QByteArray> secret = openSecret(board, id);
    result["secret"] = secret ? QJsonValue(base64(*secret)) : QJsonValue();
    return result;
}

QJsonObject setRole(const Invocation &call, UserIdentity &id) {
    const LoadedBoard board = loadInput(call.input);
    const std::optional<QByteArray> user = fromHex(requireOption(call, "user"), 32);
    const int role = static_cast<int>(std::ranges::find(roleNames, requireOption(call, "role")) -
                                      roleNames.begin());
    if (!user || (role != 1 && role != 2))
        throw UsageError("set-role needs --user <hex key> and --role member|owner");
    const std::optional<QByteArray> secret = openSecret(board, id);
    if (!secret)
        throw RefusedError("you hold no secret of this board to hand on");
    try {
        const QString prefix = "boards/" + board.info.board.toString(QUuid::WithoutBraces) + "/";
        return {{"files",
                 filesJson(repository::setRole(board, authorOf(id), *secret, *user,
                                               call.options.value("name"), static_cast<Role>(role)),
                           prefix)}};
    } catch (const std::invalid_argument &error) {
        throw RefusedError(error.what());
    }
}

// --clients a,b,c: the Yrs client ids this device used on the board.
std::vector<std::uint64_t> clientsOption(const Invocation &call) {
    std::vector<std::uint64_t> clients;
    for (const QString &each : call.options.value("clients").split(',', Qt::SkipEmptyParts)) {
        bool ok = false;
        clients.push_back(each.trimmed().toULongLong(&ok));
        if (!ok)
            throw UsageError("--clients takes client ids separated by commas");
    }
    return clients;
}

QString prefixOf(const LoadedBoard &board) {
    return "boards/" + board.info.board.toString(QUuid::WithoutBraces) + "/";
}

QJsonObject removeUser(const Invocation &call, UserIdentity &id) {
    const LoadedBoard board = loadInput(call.input);
    const std::optional<QByteArray> user = fromHex(requireOption(call, "user"), 32);
    if (!user)
        throw UsageError("remove needs --user <hex key>");
    if (board.access.role(*user) == Role::None)
        throw RefusedError("that person has no role on this board");
    try {
        return {{"files", filesJson(removeMember(*documentOf(board), board, authorOf(id), *user,
                                                 clientsOption(call), freshCapability()),
                                    prefixOf(board))}};
    } catch (const std::invalid_argument &error) {
        throw RefusedError(error.what());
    }
}

// The bytes in change files that the snapshots do not cover yet: what compacting would fold.
qint64 uncoveredBytes(const LoadedBoard &board, const BoardFiles &files) {
    qint64 total = 0;
    for (const AcceptedContent &change : board.changes)
        if (!changesCovered(board, change.device))
            total += files.at(change.path).size();
    return total;
}

// compact [--above BYTES] [--clients ...]: folds everything into one snapshot when the
// change files the snapshots do not cover hold more than BYTES (default 0), and otherwise
// deletes this device's change file once snapshots cover it.
QJsonObject compactBoard(const Invocation &call, UserIdentity &id) {
    const auto [files, access] = boardInput(call.input);
    const LoadedBoard board = loadFiles(files, access);
    bool ok = true;
    const qint64 above = call.options.value("above", "0").toLongLong(&ok);
    if (!ok || above < 0)
        throw UsageError("--above takes a number of bytes");
    const qint64 bytes = uncoveredBytes(board, files);
    const Author author = authorOf(id);
    if (bytes > above && board.access.role(author.userKey) >= Role::Member) {
        const Compaction done = compact(*documentOf(board), board, author, clientsOption(call));
        return {{"files", filesJson(done.files, prefixOf(board))},
                {"compacted", true},
                {"changeBytes", bytes}};
    }
    FileChanges tidy;
    const QString own = changeFilePath(author.certificate.deviceKey);
    if (files.contains(own) && changesCovered(board, author.certificate.deviceKey))
        tidy[own] = std::nullopt;
    return {
        {"files", filesJson(tidy, prefixOf(board))}, {"compacted", false}, {"changeBytes", bytes}};
}

using Command = std::function<QJsonObject(const Invocation &, UserIdentity &)>;

const std::map<QString, std::pair<Command, bool>> &commands() {
    static const std::map<QString, std::pair<Command, bool>> table{
        {QStringLiteral("identity"), {identity, false}},
        {QStringLiteral("identity-import"), {importIdentity, false}},
        {QStringLiteral("device-secrets"), {deviceSecrets, false}},
        {QStringLiteral("person"), {person, false}},
        {QStringLiteral("create"), {create, false}},
        {QStringLiteral("list"), {list, true}},
        {QStringLiteral("open"), {open, true}},
        {QStringLiteral("set-role"), {setRole, true}},
        {QStringLiteral("remove"), {removeUser, true}},
        {QStringLiteral("compact"), {compactBoard, true}}};
    return table;
}

} // namespace

QJsonObject runCommand(const Invocation &invocation, UserIdentity &identity) {
    const auto found = commands().find(invocation.command);
    if (found == commands().end())
        throw UsageError("unknown command " + invocation.command.toStdString());
    return found->second.first(invocation, identity);
}

bool readsInput(const QString &command) {
    const auto found = commands().find(command);
    return found != commands().end() && found->second.second;
}

} // namespace peerchalk::tools
