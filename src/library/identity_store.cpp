#include "library/identity_store.hpp"

#include "library/files.hpp"

#include <p2pdc/recipient_crypto.h>
#include <p2pdc/security.h>

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <variant>

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

template <class T> T unwrap(p2pdc::CryptoResult<T> result, const char *what) {
    if (auto *value = std::get_if<T>(&result))
        return std::move(*value);
    throw std::runtime_error(what);
}

QByteArray toBytes(std::span<const std::byte> bytes) {
    return {reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size())};
}

p2pdc::ByteView view(const QByteArray &bytes) {
    return {reinterpret_cast<const std::byte *>(bytes.constData()),
            static_cast<std::size_t>(bytes.size())};
}

// A buffer for secret material that is overwritten when it goes out of scope,
// including when an exception unwinds.
struct SecretBytes {
    QByteArray bytes;
    explicit SecretBytes(QByteArray value) : bytes(std::move(value)) {}
    SecretBytes(const SecretBytes &) = delete;
    SecretBytes &operator=(const SecretBytes &) = delete;
    ~SecretBytes() { bytes.fill('\0'); }

    [[nodiscard]] std::span<std::byte> span() {
        return {reinterpret_cast<std::byte *>(bytes.data()),
                static_cast<std::size_t>(bytes.size())};
    }
};

Verification verificationFromName(const QString &name) {
    const auto it = std::ranges::find(verificationNames, name);
    if (it == verificationNames.end())
        throw std::runtime_error("unknown verification \"" + name.toStdString() + "\"");
    return static_cast<Verification>(it - verificationNames.begin());
}

QString identityPath(const QString &directory) {
    return QDir(directory).filePath(QStringLiteral("identity.json"));
}

} // namespace

IdentityStore::IdentityStore(QString directory, const QString &defaultName)
    : directory_(std::move(directory)) {
    loadIdentity(defaultName);
    loadContacts();
}

void IdentityStore::loadIdentity(const QString &defaultName) {
    const QString path = identityPath(directory_);
    const std::optional<QJsonObject> secrets = readJsonObject(path, formatVersion);
    if (!secrets) {
        createIdentity(defaultName);
        return;
    }
    try {
        const SecretBytes seed{base64Field(*secrets, QStringLiteral("signingSeed"), 32)};
        const p2pdc::Identity identity =
            unwrap(p2pdc::Identity::import_seed(view(seed.bytes)), "the signing seed is unusable");
        const SecretBytes recipientSecret{
            base64Field(*secrets, QStringLiteral("recipientSecret"), 32)};
        unwrap(p2pdc::RecipientKeyPair::import_secret_key(view(recipientSecret.bytes)),
               "the recipient secret is unusable");
        signingKey_ = toBytes(identity.public_key());
    } catch (const std::exception &error) {
        throw std::runtime_error(path.toStdString() + ": " + error.what());
    }
    const std::optional<QJsonObject> profile =
        readJsonObject(QDir(directory_).filePath(QStringLiteral("profile.json")), formatVersion);
    displayName_ = profile ? profile->value(QStringLiteral("name")).toString().trimmed() : "";
    if (displayName_.isEmpty())
        displayName_ = requireName(defaultName);
    colorIndex_ = profile ? profile->value(QStringLiteral("color")).toInt() % colorCount : 0;
}

void IdentityStore::createIdentity(const QString &defaultName) {
    const p2pdc::Identity identity =
        unwrap(p2pdc::Identity::generate(), "Could not generate a signing identity");
    const p2pdc::RecipientKeyPair recipient =
        unwrap(p2pdc::RecipientKeyPair::generate(), "Could not generate a recipient key");
    SecretBytes seed{QByteArray(32, '\0')};
    SecretBytes secret{QByteArray(32, '\0')};
    unwrap(identity.export_seed(seed.span()), "Could not export the signing seed");
    unwrap(recipient.export_secret_key(secret.span()), "Could not export the recipient key");
    signingKey_ = toBytes(identity.public_key());
    displayName_ = requireName(defaultName);
    saveIdentity(seed.bytes, secret.bytes);
    saveProfile();
}

void IdentityStore::saveIdentity(const QByteArray &signingSeed,
                                 const QByteArray &recipientSecret) const {
    writeJsonObject(
        identityPath(directory_),
        {{QStringLiteral("signingSeed"), QString::fromLatin1(signingSeed.toBase64())},
         {QStringLiteral("recipientSecret"), QString::fromLatin1(recipientSecret.toBase64())}},
        formatVersion, true);
}

void IdentityStore::saveProfile() const {
    writeJsonObject(
        QDir(directory_).filePath(QStringLiteral("profile.json")),
        {{QStringLiteral("name"), displayName_}, {QStringLiteral("color"), colorIndex_}},
        formatVersion, false);
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
    if (card.signingKey == signingKey_)
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
