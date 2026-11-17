#include "library/identity_store.hpp"

#include "library/files.hpp"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr int formatVersion = 1;
constexpr int colorCount = 5;
const std::array<QString, 4> verificationNames = {QStringLiteral("none"),
                                                  QStringLiteral("in-person"),
                                                  QStringLiteral("call"), QStringLiteral("shared")};

QString requireName(const QString &name) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        throw std::invalid_argument("Names cannot be empty");
    return trimmed;
}

Verification verificationFromName(const QString &name) {
    const auto it = std::ranges::find(verificationNames, name);
    if (it == verificationNames.end())
        throw std::runtime_error("unknown verification \"" + name.toStdString() + "\"");
    return static_cast<Verification>(it - verificationNames.begin());
}

} // namespace

IdentityStore::IdentityStore(QString directory, const QString &defaultName)
    : directory_(std::move(directory)), identity_(directory_) {
    loadProfile(defaultName);
    loadContacts();
}

void IdentityStore::loadProfile(const QString &defaultName) {
    const std::optional<QJsonObject> profile =
        readJsonObject(QDir(directory_).filePath(QStringLiteral("profile.json")), formatVersion);
    displayName_ = profile ? profile->value(QStringLiteral("name")).toString().trimmed() : "";
    colorIndex_ = profile ? profile->value(QStringLiteral("color")).toInt() % colorCount : 0;
    if (displayName_.isEmpty()) {
        displayName_ = requireName(defaultName);
        saveProfile();
    }
}

void IdentityStore::saveProfile() const {
    writeJsonObject(
        QDir(directory_).filePath(QStringLiteral("profile.json")),
        {{QStringLiteral("name"), displayName_}, {QStringLiteral("color"), colorIndex_}},
        formatVersion, false);
}

void IdentityStore::adoptIdentity(const QString &transferCode) {
    const QString name = identity_.adopt(transferCode);
    if (!name.isEmpty())
        displayName_ = name;
    std::erase_if(contacts_, [this](const Contact &c) { return c.signingKey == signingKey(); });
    saveProfile();
    saveContacts();
}

void IdentityStore::loadContacts() {
    const QString path = QDir(directory_).filePath(QStringLiteral("contacts.json"));
    const std::optional<QJsonObject> root = readJsonObject(path, formatVersion);
    if (!root)
        return;
    try {
        for (const QJsonValue &value : root->value(QStringLiteral("contacts")).toArray()) {
            const QJsonObject object = value.toObject();
            contacts_.push_back(
                {base64Field(object, QStringLiteral("signingKey"), signingKeySize),
                 requireName(object.value(QStringLiteral("name")).toString()),
                 verificationFromName(object.value(QStringLiteral("verification")).toString())});
        }
    } catch (const std::exception &error) {
        throw std::runtime_error(path.toStdString() + ": " + error.what());
    }
}

void IdentityStore::saveContacts() const {
    QJsonArray contacts;
    for (const Contact &contact : contacts_) {
        contacts.append(QJsonObject{
            {QStringLiteral("signingKey"), QString::fromLatin1(contact.signingKey.toBase64())},
            {QStringLiteral("name"), contact.name},
            {QStringLiteral("verification"),
             verificationNames.at(static_cast<std::size_t>(contact.verification))}});
    }
    writeJsonObject(QDir(directory_).filePath(QStringLiteral("contacts.json")),
                    {{QStringLiteral("contacts"), contacts}}, formatVersion, false);
}

void IdentityStore::setDisplayName(const QString &name) {
    displayName_ = requireName(name);
    saveProfile();
}

void IdentityStore::setColorIndex(int index) {
    if (index < 0 || index >= colorCount)
        throw std::invalid_argument("Unknown colour");
    colorIndex_ = index;
    saveProfile();
}

Contact &IdentityStore::findContact(const QByteArray &signingKey) {
    const auto it = std::ranges::find(contacts_, signingKey, &Contact::signingKey);
    if (it == contacts_.end())
        throw std::invalid_argument("Unknown person");
    return *it;
}

void IdentityStore::addContact(const ContactCard &card) {
    if (card.signingKey.size() != signingKeySize)
        throw std::invalid_argument("A contact needs a 32-byte signing key");
    if (card.signingKey == signingKey())
        throw std::invalid_argument("That is your own key");
    const auto it = std::ranges::find(contacts_, card.signingKey, &Contact::signingKey);
    if (it != contacts_.end())
        it->name = requireName(card.name);
    else
        contacts_.push_back({card.signingKey, requireName(card.name), Verification::None});
    saveContacts();
}

void IdentityStore::setVerification(const QByteArray &signingKey, Verification verification) {
    findContact(signingKey).verification = verification;
    saveContacts();
}

void IdentityStore::removeContact(const QByteArray &signingKey) {
    static_cast<void>(findContact(signingKey)); // throws for unknown people
    std::erase_if(contacts_,
                  [&signingKey](const Contact &c) { return c.signingKey == signingKey; });
    saveContacts();
}

} // namespace peerchalk
