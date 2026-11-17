// Library: members, sharing, contacts and identity linking.
#include "ui/library.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace peerchalk {
namespace {

const std::array<QString, 4> verificationNames = {QStringLiteral("none"),
                                                  QStringLiteral("in-person"),
                                                  QStringLiteral("call"), QStringLiteral("shared")};

QUuid parseId(const QString &id) {
    const QUuid uuid = QUuid::fromString(id);
    if (uuid.isNull())
        throw std::invalid_argument("Unknown item");
    return uuid;
}

QByteArray parseKey(const QString &keyHex) {
    const QByteArray key = QByteArray::fromHex(keyHex.toLatin1());
    if (key.size() != signingKeySize)
        throw std::invalid_argument("Unknown person");
    return key;
}

const std::array<QString, 4> roleNames = {QStringLiteral("none"), QStringLiteral("member"),
                                          QStringLiteral("owner"), QStringLiteral("admin")};

QString roleName(Role role) { return roleNames.at(static_cast<std::size_t>(role)); }

Role parseRole(const QString &name) {
    const auto it = std::ranges::find(roleNames, name);
    if (it == roleNames.end() || name == QStringLiteral("admin"))
        throw std::invalid_argument("Unknown role");
    return static_cast<Role>(it - roleNames.begin());
}

QVariantMap person(const QByteArray &key, const QString &name, Role role, bool you) {
    return {{"key", QString::fromLatin1(key.toHex())},
            {"name", name},
            {"fingerprint", formatFingerprint(key)},
            {"role", roleName(role)},
            {"you", you}};
}

} // namespace

// What you call someone: their name in your contacts, else the one given when they got
// their role.
QString Library::nameOf(const QByteArray &key, const AccessState &state) const {
    if (key == workspace_.user())
        return identity_.displayName();
    const auto contact = std::ranges::find(identity_.contacts(), key, &Contact::signingKey);
    if (contact != identity_.contacts().end())
        return contact->name;
    const auto named = state.names.find(key);
    return named != state.names.end() && !named->second.isEmpty() ? named->second : tr("Someone");
}

// The admin, owners and members (highest role first), then contacts without a role.
QVariantList Library::people(const AccessState &state) {
    std::vector<std::pair<QByteArray, Role>> holders(state.roles.begin(), state.roles.end());
    std::ranges::stable_sort(holders, std::greater{}, &std::pair<QByteArray, Role>::second);
    QVariantList result;
    for (const auto &[key, role] : holders)
        result.append(person(key, nameOf(key, state), role, key == workspace_.user()));
    for (const Contact &contact : identity_.contacts())
        if (state.role(contact.signingKey) == Role::None)
            result.append(person(contact.signingKey, contact.name, Role::None, false));
    return result;
}

QString Library::projectRole(const QString &projectId) {
    return roleName(workspace_.projectRole(QUuid::fromString(projectId), workspace_.user()));
}

QString Library::boardRole(const QString &boardId) {
    return roleName(workspace_.boardRole(QUuid::fromString(boardId), workspace_.user()));
}

QVariantList Library::projectPeople(const QString &projectId) {
    QVariantList result;
    attempt([&] { result = people(workspace_.project(parseId(projectId)).state); });
    return result;
}

QVariantList Library::boardPeople(const QString &boardId) {
    QVariantList result;
    attempt([&] { result = people(workspace_.board(parseId(boardId)).state); });
    return result;
}

void Library::setProjectRole(const QString &projectId, const QString &keyHex, const QString &role) {
    const bool changed = attempt([&] {
        const QUuid id = parseId(projectId);
        const QByteArray key = parseKey(keyHex);
        workspace_.setProjectRole(id, key, nameOf(key, workspace_.project(id).state),
                                  parseRole(role));
    });
    if (changed)
        workspaceUpdated();
}

void Library::setBoardRole(const QString &boardId, const QString &keyHex, const QString &role) {
    const bool changed = attempt([&] {
        const QUuid id = parseId(boardId);
        const QByteArray key = parseKey(keyHex);
        workspace_.setBoardRole(id, key, nameOf(key, workspace_.board(id).state), parseRole(role));
    });
    if (changed)
        workspaceUpdated();
}

void Library::transferProjectAdmin(const QString &projectId, const QString &keyHex) {
    const bool changed = attempt([&] {
        const QUuid id = parseId(projectId);
        const QByteArray key = parseKey(keyHex);
        workspace_.transferProjectAdmin(id, key, nameOf(key, workspace_.project(id).state));
    });
    if (changed)
        workspaceUpdated();
}

void Library::transferBoardAdmin(const QString &boardId, const QString &keyHex) {
    const bool changed = attempt([&] {
        const QUuid id = parseId(boardId);
        const QByteArray key = parseKey(keyHex);
        workspace_.transferBoardAdmin(id, key, nameOf(key, workspace_.board(id).state));
    });
    if (changed)
        workspaceUpdated();
}

void Library::ensureContact(const QString &keyHex, const QString &name) {
    const QByteArray key = parseKey(keyHex);
    if (std::ranges::find(identity_.contacts(), key, &Contact::signingKey) ==
        identity_.contacts().end()) {
        identity_.addContact({key, name.trimmed().isEmpty() ? tr("Someone") : name});
        refreshContacts();
    }
}

void Library::admitToProject(const QString &projectId, const QString &keyHex, const QString &name) {
    if (attempt([&] { ensureContact(keyHex, name); }))
        setProjectRole(projectId, keyHex, QStringLiteral("member"));
}

void Library::admitToBoard(const QString &boardId, const QString &keyHex, const QString &name) {
    if (attempt([&] { ensureContact(keyHex, name); }))
        setBoardRole(boardId, keyHex, QStringLiteral("member"));
}

// Projects you own (to add the board, which you must own too) and, for removal, projects
// whose admin you are.
QVariantList Library::boardProjects(const QString &boardId) {
    QVariantList result;
    attempt([&] {
        const QUuid id = parseId(boardId);
        const bool ownBoard = workspace_.boardRole(id, workspace_.user()) >= Role::Owner;
        for (const ProjectEntry &project : workspace_.projects()) {
            const Role role = workspace_.projectRole(project.id, workspace_.user());
            const bool included =
                std::ranges::find(project.state.boards, id) != project.state.boards.end();
            if ((ownBoard && role >= Role::Owner) || (included && role == Role::Admin))
                result.append(QVariantMap{{"projectId", project.id.toString(QUuid::WithoutBraces)},
                                          {"name", project.name()},
                                          {"included", included}});
        }
    });
    return result;
}

void Library::setBoardInProject(const QString &boardId, const QString &projectId, bool included) {
    if (attempt(
            [&] { workspace_.setBoardInProject(parseId(boardId), parseId(projectId), included); }))
        workspaceUpdated();
}

bool Library::addContact(const QString &code) {
    const bool added = attempt([&] {
        const std::optional<ContactCard> card = decodeContactCard(code);
        if (!card)
            throw std::invalid_argument("That is not a PeerChalk contact code");
        identity_.addContact(*card);
    });
    if (added)
        refreshContacts();
    return added;
}

void Library::setVerification(const QString &keyHex, const QString &how) {
    const bool changed = attempt([&] {
        const auto it = std::ranges::find(verificationNames, how);
        if (it == verificationNames.end())
            throw std::invalid_argument("Unknown way of verifying");
        identity_.setVerification(parseKey(keyHex),
                                  static_cast<Verification>(it - verificationNames.begin()));
    });
    if (changed)
        refreshContacts();
}

void Library::removeContact(const QString &keyHex) {
    if (attempt([&] { identity_.removeContact(parseKey(keyHex)); }))
        refreshContacts();
}

QString Library::identityTransferCode() {
    QString code;
    attempt([&] { code = identity_.identity().transferCode(identity_.displayName()); });
    return code;
}

bool Library::adoptIdentity(const QString &code) {
    const bool adopted = attempt([&] {
        if (!canAdoptIdentity())
            throw std::invalid_argument(
                "This device already has projects or boards of its own identity");
        identity_.adoptIdentity(code);
        workspace_.resetUser(identity_.signingKey());
    });
    if (adopted) {
        refreshContacts();
        emit profileChanged();
    }
    return adopted;
}

} // namespace peerchalk
