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

QVariantMap person(const QByteArray &key, const QString &name, bool member, bool admin, bool you) {
    return {{"key", QString::fromLatin1(key.toHex())},
            {"name", name},
            {"fingerprint", formatFingerprint(key)},
            {"member", member || admin},
            {"admin", admin},
            {"you", you}};
}

} // namespace

// The admin, the members, then contacts who are not members (so they can be added).
QVariantList Library::people(const Manifest &manifest) {
    QVariantList result;
    const QByteArray you = workspace_.user();
    const auto nameOf = [&](const QByteArray &key) -> QString {
        if (key == you)
            return identity_.displayName();
        const auto contact = std::ranges::find(identity_.contacts(), key, &Contact::signingKey);
        if (contact != identity_.contacts().end())
            return contact->name;
        const auto member = std::ranges::find(manifest.members, key, &Member::key);
        return member != manifest.members.end() ? member->name : tr("Admin");
    };
    result.append(
        person(manifest.admin, nameOf(manifest.admin), true, true, manifest.admin == you));
    for (const Member &member : manifest.members)
        result.append(person(member.key, nameOf(member.key), true, false, member.key == you));
    for (const Contact &contact : identity_.contacts()) {
        if (!manifest.includes(contact.signingKey))
            result.append(person(contact.signingKey, contact.name, false, false, false));
    }
    return result;
}

QVariantList Library::projectPeople(const QString &projectId) {
    QVariantList result;
    attempt([&] {
        const ProjectEntry &project = workspace_.project(parseId(projectId));
        if (project.manifest)
            result = people(project.manifest->manifest);
    });
    return result;
}

QVariantList Library::boardPeople(const QString &boardId) {
    QVariantList result;
    attempt([&] {
        const BoardEntry &board = workspace_.board(parseId(boardId));
        if (board.manifest)
            result = people(board.manifest->manifest);
    });
    return result;
}

std::vector<Member> Library::withMember(std::vector<Member> members, const QString &keyHex,
                                        bool member) const {
    const QByteArray key = parseKey(keyHex);
    std::erase_if(members, [&](const Member &m) { return m.key == key; });
    if (member) {
        const auto contact = std::ranges::find(identity_.contacts(), key, &Contact::signingKey);
        if (contact == identity_.contacts().end())
            throw std::invalid_argument("Add this person to your known people first");
        members.push_back({key, contact->name});
    }
    return members;
}

void Library::setProjectMember(const QString &projectId, const QString &keyHex, bool member) {
    const bool changed = attempt([&] {
        const QUuid id = parseId(projectId);
        const ProjectEntry &project = workspace_.project(id);
        if (!project.manifest)
            throw std::invalid_argument("This project has not arrived yet");
        workspace_.setProjectMembers(
            id, withMember(project.manifest->manifest.members, keyHex, member));
    });
    if (changed)
        workspaceUpdated();
}

void Library::setBoardMember(const QString &boardId, const QString &keyHex, bool member) {
    const bool changed = attempt([&] {
        const QUuid id = parseId(boardId);
        const BoardEntry &board = workspace_.board(id);
        if (!board.manifest)
            throw std::invalid_argument("This board has not arrived yet");
        workspace_.setBoardMembers(id,
                                   withMember(board.manifest->manifest.members, keyHex, member));
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
        setProjectMember(projectId, keyHex, true);
}

void Library::admitToBoard(const QString &boardId, const QString &keyHex, const QString &name) {
    if (attempt([&] { ensureContact(keyHex, name); }))
        setBoardMember(boardId, keyHex, true);
}

QVariantList Library::boardProjects(const QString &boardId) {
    QVariantList result;
    attempt([&] {
        const BoardEntry &board = workspace_.board(parseId(boardId));
        for (const ProjectEntry &project : workspace_.projects()) {
            if (!workspace_.mayAccessProject(workspace_.user(), project.id))
                continue;
            const bool included =
                board.manifest &&
                std::ranges::find(board.manifest->manifest.projects, project.id) !=
                    board.manifest->manifest.projects.end();
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
