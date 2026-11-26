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

// Ada's board with Ben as a member: the input, and Ben's key.
struct SharedBoard {
    QString board;
    QJsonObject input;
    QString benKey;
};

SharedBoard shareWithBen(Person &ada, Person &ben) {
    const QJsonObject created = run(ada, "create", {{"name", "Lecture"}});
    SharedBoard shared{created["board"].toString(), {}, run(ben, "identity")["userKey"].toString()};
    shared.input = boardInput(created["files"].toObject(), shared.board);
    const QJsonObject appointed =
        run(ada, "set-role", {{"user", shared.benKey}, {"role", "member"}, {"name", "Ben"}},
            shared.input);
    shared.input = merged(
        shared.input, boardInput(appointed["files"].toObject(), shared.board)["files"].toObject());
    return shared;
}

// Applies an answer's files ({path: base64 or null}) to a board's input.
QJsonObject applied(QJsonObject input, const QJsonObject &answer, const QString &board) {
    QJsonObject files = input["files"].toObject();
    const QJsonObject changes = boardInput(answer["files"].toObject(), board)["files"].toObject();
    for (auto it = changes.begin(); it != changes.end(); ++it) {
        if (it.value().isNull())
            files.remove(it.key());
        else
            files[it.key()] = it.value();
    }
    input["files"] = files;
    return input;
}

TEST(BoardTool, RemovingSomeoneSealsANewSecretToTheOthers) {
    Person ada;
    Person ben;
    SharedBoard shared = shareWithBen(ada, ben);
    const QJsonValue before = run(ada, "open", {}, shared.input)["secret"];
    EXPECT_THROW((void)run(ben, "remove", {{"user", run(ada, "identity")["userKey"].toString()}},
                           shared.input),
                 RefusedError); // a member removes nobody
    const QJsonObject removed =
        run(ada, "remove", {{"user", shared.benKey}, {"clients", "7,8"}}, shared.input);
    shared.input = applied(shared.input, removed, shared.board);
    const QJsonObject opened = run(ada, "open", {}, shared.input);
    EXPECT_EQ(opened["epoch"].toInt(), 1);
    EXPECT_NE(opened["secret"], before);
    EXPECT_EQ(opened["snapshots"].toInt(), 1);
    EXPECT_EQ(run(ben, "open", {}, shared.input)["role"].toString(), QStringLiteral("none"));
    EXPECT_THROW((void)run(ada, "remove", {{"user", shared.benKey}}, shared.input), RefusedError);
    EXPECT_THROW((void)run(ada, "remove", {}, shared.input), UsageError);
}

TEST(BoardTool, CompactsOnlyAboveTheThresholdAndThenTidies) {
    Person ada;
    Person ben;
    SharedBoard shared = shareWithBen(ada, ben);
    const QJsonObject kept = run(ada, "compact", {{"above", "1000000"}}, shared.input);
    EXPECT_FALSE(kept["compacted"].toBool());
    EXPECT_GT(kept["changeBytes"].toInt(), 0); // the change file holding the board's name
    EXPECT_TRUE(kept["files"].toObject().isEmpty());

    EXPECT_THROW((void)run(ada, "compact", {{"clients", "x"}}, shared.input), UsageError);
    const QJsonObject compacted = run(ada, "compact", {{"clients", "7"}}, shared.input);
    EXPECT_TRUE(compacted["compacted"].toBool());
    shared.input = applied(shared.input, compacted, shared.board);
    const QJsonObject opened = run(ada, "open", {}, shared.input);
    EXPECT_EQ(opened["snapshots"].toInt(), 1);
    EXPECT_EQ(opened["changes"].toInt(), 0);
    EXPECT_EQ(opened["name"].toString(), QStringLiteral("Lecture"));
    const QJsonObject again = run(ada, "compact", {}, shared.input);
    EXPECT_FALSE(again["compacted"].toBool()); // nothing left to fold
    EXPECT_EQ(again["changeBytes"].toInt(), 0);
    EXPECT_THROW((void)run(ada, "compact", {{"above", "-1"}}, shared.input), UsageError);
}

} // namespace
} // namespace peerchalk::tools
