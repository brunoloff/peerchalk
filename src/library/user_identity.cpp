#include "library/user_identity.hpp"

#include "library/codes.hpp"
#include "library/files.hpp"
#include "library/secrets.hpp"

#include <p2pdc/recipient_crypto.h>

#include <QCryptographicHash>
#include <QDir>
#include <QJsonObject>
#include <QSysInfo>

namespace peerchalk {
namespace {

constexpr int formatVersion = 1;
constexpr qsizetype seedSize = 32;
const QString transferPrefix = QStringLiteral("peerchalk-identity:1:");

QString path(const QString &directory, const char *name) {
    return QDir(directory).filePath(QString::fromLatin1(name));
}

QString base64(const QByteArray &bytes) { return QString::fromLatin1(bytes.toBase64()); }

QString newDeviceName() {
#ifdef __EMSCRIPTEN__
    return QStringLiteral("Web browser"); // the host name is only "emscripten" here
#else
    return QSysInfo::machineHostName();
#endif
}

QByteArray newSigningSeed() {
    const p2pdc::Identity identity =
        unwrap(p2pdc::Identity::generate(), "Could not generate a signing identity");
    SecretBytes seed{QByteArray(seedSize, '\0')};
    unwrap(identity.export_seed(seed.span()), "Could not export a signing seed");
    return seed.bytes; // copied out; the caller wraps it in SecretBytes
}

QByteArray newRecipientSecret() {
    const p2pdc::RecipientKeyPair pair =
        unwrap(p2pdc::RecipientKeyPair::generate(), "Could not generate a recipient key");
    SecretBytes secret{QByteArray(seedSize, '\0')};
    unwrap(pair.export_secret_key(secret.span()), "Could not export a recipient key");
    return secret.bytes;
}

QByteArray publicKey(const SecretBytes &seed) {
    const p2pdc::Identity identity =
        unwrap(p2pdc::Identity::import_seed(seed.view()), "the signing seed is unusable");
    return toBytes(identity.public_key());
}

} // namespace

UserIdentity::UserIdentity(QString directory) : directory_(std::move(directory)) {
    loadUser();
    loadDevice();
}

UserIdentity::~UserIdentity() = default;

void UserIdentity::useUserSeed(const QByteArray &seedBytes) {
    const SecretBytes seed{seedBytes};
    user_ = std::make_unique<p2pdc::Identity>(
        unwrap(p2pdc::Identity::import_seed(seed.view()), "the user seed is unusable"));
    userKey_ = toBytes(user_->public_key());
    QCryptographicHash hash(QCryptographicHash::Blake2b_256);
    hash.addData(QByteArrayView("peerchalk/personal-capability/v1"));
    hash.addData(seed.bytes);
    personalCapability_ = hash.result();
}

void UserIdentity::loadUser() {
    const QString file = path(directory_, "identity.json");
    const std::optional<QJsonObject> stored = readJsonObject(file, formatVersion);
    if (!stored) {
        const SecretBytes seed{newSigningSeed()};
        writeJsonObject(file, {{"signingSeed", base64(seed.bytes)}}, formatVersion, true);
        useUserSeed(seed.bytes);
        return;
    }
    try {
        const SecretBytes seed{base64Field(*stored, QStringLiteral("signingSeed"), seedSize)};
        useUserSeed(seed.bytes);
    } catch (const std::exception &error) {
        throw std::runtime_error(file.toStdString() + ": " + error.what());
    }
}

void UserIdentity::loadDevice() {
    const QString file = path(directory_, "device.json");
    std::optional<QJsonObject> stored = readJsonObject(file, formatVersion);
    if (!stored) {
        const SecretBytes seed{newSigningSeed()};
        const SecretBytes secret{newRecipientSecret()};
        stored = QJsonObject{{"signingSeed", base64(seed.bytes)},
                             {"recipientSecret", base64(secret.bytes)},
                             {"name", newDeviceName()}};
        writeJsonObject(file, *stored, formatVersion, true);
    }
    try {
        const SecretBytes seed{base64Field(*stored, QStringLiteral("signingSeed"), seedSize)};
        const SecretBytes secret{base64Field(*stored, QStringLiteral("recipientSecret"), seedSize)};
        unwrap(p2pdc::RecipientKeyPair::import_secret_key(secret.view()),
               "the recipient secret is unusable");
        deviceKey_ = publicKey(seed);
        deviceName_ = (*stored)[QStringLiteral("name")].toString();
    } catch (const std::exception &error) {
        throw std::runtime_error(file.toStdString() + ": " + error.what());
    }
}

QByteArray UserIdentity::sign(const QByteArray &message) const {
    const p2pdc::Signature signature =
        unwrap(user_->sign(byteView(message)), "Could not sign with the user key");
    return toBytes(signature);
}

Signer UserIdentity::signer() const {
    return [this](const QByteArray &message) { return sign(message); };
}

DeviceCertificate UserIdentity::deviceCertificate() const {
    return certifyDevice(userKey_, deviceKey_, deviceName_, signer());
}

void UserIdentity::withDeviceSecrets(
    const std::function<void(std::span<const std::byte>, std::span<const std::byte>)> &use) const {
    const QString file = path(directory_, "device.json");
    const std::optional<QJsonObject> stored = readJsonObject(file, formatVersion);
    if (!stored)
        throw std::runtime_error(file.toStdString() + ": missing");
    const SecretBytes seed{base64Field(*stored, QStringLiteral("signingSeed"), seedSize)};
    const SecretBytes secret{base64Field(*stored, QStringLiteral("recipientSecret"), seedSize)};
    use(seed.view(), secret.view());
}

QString UserIdentity::transferCode(const QString &displayName) const {
    const std::optional<QJsonObject> stored =
        readJsonObject(path(directory_, "identity.json"), formatVersion);
    if (!stored)
        throw std::runtime_error("The identity file is missing");
    const SecretBytes seed{base64Field(*stored, QStringLiteral("signingSeed"), seedSize)};
    const SecretBytes payload{seed.bytes + displayName.trimmed().toUtf8().left(200)};
    return transferPrefix + QString::fromLatin1(payload.bytes.toBase64(
                                QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString UserIdentity::adopt(const QString &code) {
    const QString trimmed = code.trimmed();
    if (!trimmed.startsWith(transferPrefix))
        throw std::invalid_argument("That is not a PeerChalk identity code");
    const QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(
        trimmed.mid(transferPrefix.size()).toLatin1(),
        QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() < seedSize)
        throw std::invalid_argument("That identity code is damaged");
    const SecretBytes payload{decoded.decoded};
    const SecretBytes seed{payload.bytes.first(seedSize)};
    useUserSeed(seed.bytes); // validates the seed before anything is written
    writeJsonObject(path(directory_, "identity.json"), {{"signingSeed", base64(seed.bytes)}},
                    formatVersion, true);
    return QString::fromUtf8(payload.bytes.mid(seedSize)).trimmed();
}

} // namespace peerchalk
