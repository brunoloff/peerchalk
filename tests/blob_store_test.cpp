#include "library/blob_store.hpp"

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

} // namespace
} // namespace peerchalk
