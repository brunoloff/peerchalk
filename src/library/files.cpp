#include "library/files.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

#include <stdexcept>

namespace peerchalk {
namespace {

[[noreturn]] void fail(const QString &path, const QString &reason) {
    throw std::runtime_error(QStringLiteral("%1: %2").arg(path, reason).toStdString());
}

} // namespace

std::optional<QByteArray> readFile(const QString &path) {
    QFile file(path);
    if (!file.exists())
        return std::nullopt;
    if (!file.open(QIODevice::ReadOnly))
        fail(path, file.errorString());
    return file.readAll();
}

// With ownerOnly, the folder is made owner-only before writing, so the temporary file
// QSaveFile writes first is never readable by others, and the final file is restricted
// after it is committed.
void writeFile(const QString &path, const QByteArray &contents, bool ownerOnly) {
    const QString folder = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(folder))
        fail(path, QStringLiteral("cannot create its folder"));
    const auto ownerAccess = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    if (ownerOnly && !QFile::setPermissions(folder, ownerAccess | QFileDevice::ExeOwner))
        fail(folder, QStringLiteral("cannot restrict the folder's permissions"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        fail(path, file.errorString());
    if (file.write(contents) != contents.size() || !file.commit())
        fail(path, file.errorString());
    if (ownerOnly && !QFile::setPermissions(path, ownerAccess))
        fail(path, QStringLiteral("cannot restrict its permissions"));
}

void removeFile(const QString &path) {
    QFile file(path);
    if (file.exists() && !file.remove())
        fail(path, file.errorString());
}

std::optional<QJsonObject> readJsonObject(const QString &path, int version) {
    const std::optional<QByteArray> contents = readFile(path);
    if (!contents)
        return std::nullopt;
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(*contents, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        fail(path, QStringLiteral("not a valid JSON object (%1)").arg(error.errorString()));
    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("version")).toInt(-1) != version)
        fail(path, QStringLiteral("unsupported version; expected %1").arg(version));
    return object;
}

void writeJsonObject(const QString &path, QJsonObject object, int version, bool ownerOnly) {
    object.insert(QStringLiteral("version"), version);
    writeFile(path, QJsonDocument(object).toJson(QJsonDocument::Indented), ownerOnly);
}

QByteArray base64Field(const QJsonObject &object, const QString &key, qsizetype size) {
    const QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(
        object.value(key).toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() != size)
        throw std::runtime_error(QStringLiteral("field \"%1\" is not %2 bytes of base64")
                                     .arg(key)
                                     .arg(size)
                                     .toStdString());
    return decoded.decoded;
}

} // namespace peerchalk
