#include "library/files.hpp"

#include <QDir>
#include <QJsonObject>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <stdexcept>

namespace peerchalk {
namespace {

TEST(Files, ReportsTheVersionOfAJsonFile) {
    QTemporaryDir dir;
    const QString path = QDir(dir.path()).filePath(QStringLiteral("data.json"));
    EXPECT_FALSE(jsonFileVersion(path).has_value());
    writeJsonObject(path, QJsonObject{{"a", 1}}, 2, false);
    EXPECT_EQ(jsonFileVersion(path), 2);
    EXPECT_THROW(static_cast<void>(readJsonObject(path, 3)), std::runtime_error);
    EXPECT_EQ(readJsonObject(path, 2)->value(QStringLiteral("a")).toInt(), 1);
}

TEST(Files, RejectsFilesThatAreNotJsonObjects) {
    QTemporaryDir dir;
    const QString path = QDir(dir.path()).filePath(QStringLiteral("data.json"));
    writeFile(path, QByteArray("[1, 2]"), false);
    EXPECT_THROW(static_cast<void>(jsonFileVersion(path)), std::runtime_error);
}

} // namespace
} // namespace peerchalk
