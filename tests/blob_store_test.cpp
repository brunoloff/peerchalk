#include "library/blob_store.hpp"

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

TEST(BlobStore, KeepsBlobsByTheirHashAndSharesThem) {
    QTemporaryDir dir;
    BlobStore store(dir.path() + "/blobs");
    const QString hash = store.put("a PDF");
    EXPECT_TRUE(BlobStore::isHash(hash));
    EXPECT_EQ(hash, BlobStore::hashOf("a PDF"));
    EXPECT_EQ(store.put("a PDF"), hash); // already there
    EXPECT_TRUE(store.has(hash));
    const std::shared_ptr<const Bytes> data = store.get(hash);
    ASSERT_TRUE(data);
    EXPECT_EQ(data->size(), 5U);
    EXPECT_EQ(store.get(hash), data); // one copy in memory
}

TEST(BlobStore, RefusesMissingDamagedAndBadlyNamedBlobs) {
    QTemporaryDir dir;
    BlobStore store(dir.path());
    const QString missing = BlobStore::hashOf("nothing");
    EXPECT_FALSE(store.has(missing));
    EXPECT_FALSE(store.get(missing));
    EXPECT_FALSE(store.get("../secret"));
    EXPECT_FALSE(BlobStore::isHash(missing.toUpper()));
    const QString hash = store.put("original");
    QFile file(dir.path() + "/" + hash);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("changed");
    file.close();
    EXPECT_FALSE(store.get(hash)); // no longer what its name says
}

TEST(BlobStore, ForgetsBlobsUnusedForLong) {
    QTemporaryDir dir;
    BlobStore store(dir.path());
    const QString used = store.put("used");
    const QString recent = store.put("unused, but new");
    const QString old = store.put("unused for long");
    QFile oldFile(dir.path() + "/" + old);
    ASSERT_TRUE(oldFile.open(QIODevice::ReadWrite));
    ASSERT_TRUE(oldFile.setFileTime(QDateTime::currentDateTimeUtc().addDays(-40),
                                    QFileDevice::FileModificationTime));
    oldFile.close();
    QFile usedFile(dir.path() + "/" + used);
    ASSERT_TRUE(usedFile.open(QIODevice::ReadWrite));
    ASSERT_TRUE(usedFile.setFileTime(QDateTime::currentDateTimeUtc().addDays(-40),
                                     QFileDevice::FileModificationTime));
    usedFile.close();
    EXPECT_EQ(store.forgetUnused({used}, QDateTime::currentDateTimeUtc().addDays(-30)), 1);
    EXPECT_TRUE(store.has(used)); // in use, however old its file
    EXPECT_TRUE(store.has(recent));
    EXPECT_FALSE(store.has(old));
    EXPECT_EQ(store.forgetUnused({}, QDateTime::currentDateTimeUtc().addDays(-30)), 0);
    EXPECT_TRUE(store.has(used)); // using it renewed its date
}

} // namespace
} // namespace peerchalk
