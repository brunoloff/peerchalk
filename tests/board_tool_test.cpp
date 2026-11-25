#include "tools/board_tool.hpp"

#include <QJsonArray>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace peerchalk::tools {
namespace {

struct Person {
    QTemporaryDir dir;
    UserIdentity identity{dir.path()};
};

QJsonObject run(Person &person, const QString &command, QMap<QString, QString> options = {},
                const QJsonObject &input = {}, const QStringList &arguments = {}) {
    return runCommand({command, arguments, std::move(options), input}, person.identity);
}

// The "files" of a create or set-role answer as one board's input, paths under the board.
QJsonObject boardInput(const QJsonObject &files, const QString &board) {
    QJsonObject relative;
    const QString prefix = "boards/" + board + "/";
    for (auto it = files.begin(); it != files.end(); ++it)
        relative[it.key().mid(prefix.size())] = it.value();
    return {{"files", relative}, {"access", QJsonArray()}};
}

QJsonObject merged(QJsonObject input, const QJsonObject &more) {
    QJsonObject files = input["files"].toObject();
    for (auto it = more.begin(); it != more.end(); ++it)
        files[it.key()] = it.value();
    input["files"] = files;
    return input;
}

TEST(BoardTool, CreatesOpensListsAndAppoints) {
    Person ada;
    Person ben;
    const QJsonObject created = run(ada, "create", {{"name", "Lecture"}, {"display-name", "Ada"}});
    const QString board = created["board"].toString();
    QJsonObject input = boardInput(created["files"].toObject(), board);
    const QJsonObject opened = run(ada, "open", {}, input);
    EXPECT_EQ(opened["name"].toString(), QStringLiteral("Lecture"));
    EXPECT_EQ(opened["role"].toString(), QStringLiteral("admin"));
    EXPECT_EQ(opened["strokes"].toInt(), 0);
    EXPECT_FALSE(opened["secret"].toString().isEmpty());
    EXPECT_TRUE(run(ben, "open", {}, input)["secret"].isNull());

    const QString benKey = run(ben, "identity")["userKey"].toString();
    const QJsonObject appointed =
        run(ada, "set-role", {{"user", benKey}, {"role", "member"}, {"name", "Ben"}}, input);
    input = merged(input, boardInput(appointed["files"].toObject(), board)["files"].toObject());
    EXPECT_EQ(run(ben, "open", {}, input)["secret"], opened["secret"]);
    const QJsonArray boards =
        run(ben, "list", {}, {{"boards", QJsonObject{{board, input}}}})["boards"].toArray();
    ASSERT_EQ(boards.size(), 1);
    EXPECT_EQ(boards[0].toObject()["role"].toString(), QStringLiteral("member"));
    EXPECT_TRUE(boards[0].toObject()["warnings"].toArray().isEmpty());
}

TEST(BoardTool, RefusesWhatTheRulesForbid) {
    Person ada;
    Person ben;
    const QJsonObject created = run(ada, "create", {{"name", "Lecture"}});
    const QJsonObject input = boardInput(created["files"].toObject(), created["board"].toString());
    const QString adaKey = run(ada, "identity")["userKey"].toString();
    EXPECT_THROW((void)run(ben, "set-role", {{"user", adaKey}, {"role", "owner"}}, input),
                 RefusedError); // ben holds no secret and no role
    EXPECT_THROW((void)run(ada, "set-role", {{"user", "zz"}, {"role", "owner"}}, input),
                 UsageError);
    EXPECT_THROW((void)run(ada, "open", {}, QJsonObject{{"files", QJsonObject{}}}), RefusedError);
}

TEST(BoardTool, RefusesBadCommandsAndInput) {
    Person ada;
    EXPECT_THROW((void)run(ada, "frobnicate"), UsageError);
    EXPECT_THROW((void)run(ada, "create"), UsageError);
    EXPECT_THROW((void)run(ada, "identity-import", {}, {}, {"nonsense"}), UsageError);
    const QJsonObject bad{{"files", QJsonObject{{"board.json", "not base64!"}}}};
    EXPECT_THROW((void)run(ada, "open", {}, bad), UsageError);
    EXPECT_TRUE(readsInput("list"));
    EXPECT_FALSE(readsInput("identity"));
}

TEST(BoardTool, HandsOverWhatAHostedPeerChalkNeeds) {
    Person ada;
    const QJsonObject secrets = run(ada, "device-secrets", {{"name", "Ada"}});
    EXPECT_TRUE(secrets["transferCode"].toString().startsWith("peerchalk-identity:1:"));
    EXPECT_EQ(QByteArray::fromBase64(secrets["deviceSeed"].toString().toLatin1()).size(), 32);
    const QJsonObject me = run(ada, "person", {{"name", "Ada"}});
    EXPECT_TRUE(me["path"].toString().startsWith("people/"));
}

} // namespace
} // namespace peerchalk::tools
