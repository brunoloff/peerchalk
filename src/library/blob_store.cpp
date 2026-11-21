#include "library/blob_store.hpp"

#include "library/files.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <stdexcept>

namespace peerchalk {

BlobStore::BlobStore(QString directory) : directory_(std::move(directory)) {}

QString BlobStore::hashOf(const QByteArray &data) {
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool BlobStore::isHash(const QString &text) {
    return text.size() == 64 && std::ranges::all_of(text, [](QChar c) {
               return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f');
           });
}

QString BlobStore::pathOf(const QString &hash) const { return directory_ + u'/' + hash; }

QString BlobStore::put(const QByteArray &data) {
    const QString hash = hashOf(data);
    const std::lock_guard lock(lock_);
    if (QFile::exists(pathOf(hash)))
        return hash;
    if (!QDir().mkpath(directory_))
        throw std::runtime_error("Could not create the folder " + directory_.toStdString());
    writeFile(pathOf(hash), data, false);
    return hash;
}

bool BlobStore::has(const QString &hash) const {
    return isHash(hash) && QFile::exists(pathOf(hash));
}

int BlobStore::forgetUnused(const std::set<QString> &used, const QDateTime &unusedSince) {
    const std::lock_guard lock(lock_);
    int removed = 0;
    const QStringList names = QDir(directory_).entryList(QDir::Files);
    for (const QString &name : names) {
        if (!isHash(name))
            continue; // not a blob; left alone
        QFile file(pathOf(name));
        if (used.contains(name)) {
            if (file.open(QIODevice::ReadWrite)) // the date has to change for the file to say so
                file.setFileTime(QDateTime::currentDateTimeUtc(),
                                 QFileDevice::FileModificationTime);
            continue;
        }
        if (QFileInfo(file).lastModified() < unusedSince && file.remove()) {
            loaded_.erase(name);
            ++removed;
        }
    }
    return removed;
}

std::shared_ptr<const Bytes> BlobStore::get(const QString &hash) const {
    if (!isHash(hash))
        return nullptr;
    const std::lock_guard lock(lock_);
    if (std::shared_ptr<const Bytes> shared = loaded_[hash].lock())
        return shared;
    const std::optional<QByteArray> data = readFile(pathOf(hash));
    if (!data || hashOf(*data) != hash)
        return nullptr;
    const auto *begin = reinterpret_cast<const std::byte *>(data->constData());
    auto bytes = std::make_shared<const Bytes>(begin, begin + data->size());
    loaded_[hash] = bytes;
    return bytes;
}

} // namespace peerchalk
