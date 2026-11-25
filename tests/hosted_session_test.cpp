#include "hosted/hosted_session.hpp"

#include "repository/encoding.hpp"
#include "sync/network.hpp"
#include "sync/repository_scribe.hpp"
#include "ui/library.hpp"

#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

namespace peerchalk::hosted {
namespace {

// Stands in for the dashboard's frame.
class FakeBridge final : public HostedBridge {
  public:
    QList<QJsonObject> take() override { return std::exchange(inbox, {}); }
    void post(const QJsonObject &message) override { posted.append(message); }
    [[nodiscard]] std::optional<QJsonObject> last(const QString &type) const {
        for (auto it = posted.rbegin(); it != posted.rend(); ++it)
            if ((*it)["type"].toString() == type)
                return *it;
        return std::nullopt;
    }
    QList<QJsonObject> inbox;
    QList<QJsonObject> posted;
};

struct Person {
    QTemporaryDir dir;
    UserIdentity identity{dir.path()};
};

QJsonObject openMessage(const Person &person, const repository::NewBoard &board, bool canPush,
                        const QString &name) {
    QJsonObject files;
    for (const auto &[path, bytes] : board.files)
        files[path] = bytesValue(*bytes);
    QJsonObject identity{{"transferCode", person.identity.transferCode(name)},
                         {"deviceName", "test"},
                         {"name", name}};
    person.identity.withDeviceSecrets(
        [&](std::span<const std::byte> seed, std::span<const std::byte> secret) {
            identity["deviceSeed"] = bytesValue(repository::toByteArray(seed));
            identity["recipientSecret"] = bytesValue(repository::toByteArray(secret));
        });
    return {
        {"type", "open"},
        {"protocol", 1},
        {"board", QJsonObject{{"id", board.id.toString(QUuid::WithoutBraces)}, {"files", files}}},
        {"access", QJsonArray()},
        {"secret", bytesValue(board.capability)},
        {"identity", identity},
        {"canPush", canPush}};
}

class HostedSessionTest : public ::testing::Test {
  protected:
    void SetUp() override {
        board_ = repository::createBoard(QStringLiteral("Seminar"), QStringLiteral("Ada"),
                                         repository::authorOf(ada_.identity));
        auto bridge = std::make_unique<FakeBridge>();
        bridge_ = bridge.get();
        session_ = std::make_unique<HostedSession>(std::move(bridge), account_.path(),
                                                   HostedSession::Timing{10, 30, 200});
        session_->start();
    }
    // Opens the board and signs in as the app would.
    void openAndAttach(bool canPush) {
        QSignalSpy opened(session_.get(), &HostedSession::opened);
        bridge_->inbox.append(openMessage(ada_, board_, canPush, QStringLiteral("Ada")));
        ASSERT_TRUE(opened.wait(2000));
        library_ = std::make_unique<Library>(account_.path());
        sync::NetworkConfig config;
        config.enabled = false;
        network_ = std::make_unique<sync::Network>(*library_, config);
        session_->attach(*library_, *network_);
    }
    void draw() {
        Stroke stroke(0xff000000U, 2.0F);
        stroke.addPoint({0.0F, 0.0F, 1.0F});
        library_->openBoard(board_.id.toString(QUuid::WithoutBraces))->board().addStroke(stroke);
    }
    [[nodiscard]] repository::LoadedBoard loadSaved(const QJsonObject &save) const {
        repository::BoardFiles files;
        for (const auto &[path, bytes] : board_.files)
            files[path] = *bytes;
        const QJsonObject saved = save["files"].toObject();
        for (auto it = saved.begin(); it != saved.end(); ++it)
            files[it.key()] = bytesOf(it.value(), "file");
        return repository::loadBoard(files, {});
    }

    Person ada_;
    QTemporaryDir account_;
    repository::NewBoard board_;
    FakeBridge *bridge_{nullptr};
    std::unique_ptr<HostedSession> session_;
    std::unique_ptr<Library> library_;
    std::unique_ptr<sync::Network> network_;
};

TEST_F(HostedSessionTest, AloneWithPushRightsItIsItsOwnScribe) {
    ASSERT_TRUE(bridge_->last("ready"));
    openAndAttach(true);
    EXPECT_TRUE(network_->repository().isScribe(board_.id));
    draw();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        const auto save = bridge_->last("save");
        return save && (*save)["push"].toBool();
    }));
    const QJsonObject save = *bridge_->last("save");
    const repository::LoadedBoard loaded = loadSaved(save);
    EXPECT_TRUE(loaded.warnings.empty());
    Board check;
    check.apply(repository::byteView(loaded.state));
    EXPECT_EQ(check.strokes().size(), 1U);
    EXPECT_EQ(save["authors"].toArray().first().toObject()["name"].toString(), "Ada");

    bridge_->inbox.append({{"type", "pushed"},
                           {"revision", save["revision"]},
                           {"commit", QString(40, QLatin1Char('a'))}});
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return network_->repository().lastPushed(board_.id).has_value(); }));
    const qsizetype before = bridge_->posted.size();
    bridge_->inbox.append({{"type", "close"}});
    ASSERT_TRUE(QTest::qWaitFor([&] { return bridge_->last("closed").has_value(); }));
    EXPECT_FALSE((*bridge_->last("save"))["push"].toBool()); // everything was pushed
    EXPECT_GT(bridge_->posted.size(), before);
}

TEST_F(HostedSessionTest, WithoutPushRightsEditsWaitLocally) {
    openAndAttach(false);
    EXPECT_FALSE(network_->repository().isScribe(board_.id));
    draw();
    ASSERT_TRUE(QTest::qWaitFor([&] { return bridge_->last("save").has_value(); }));
    QTest::qWait(300); // longer than the idle and push delays
    for (const QJsonObject &message : bridge_->posted) {
        if (message["type"] == "save") {
            EXPECT_FALSE(message["push"].toBool());
        }
    }
    bridge_->inbox.append({{"type", "close"}});
    ASSERT_TRUE(QTest::qWaitFor([&] { return bridge_->last("closed").has_value(); }));
    EXPECT_TRUE((*bridge_->last("save"))["push"].toBool()); // asks to push what is unpushed
}

TEST_F(HostedSessionTest, RefusesBadOpensAndOutsiders) {
    QJsonObject wrong = openMessage(ada_, board_, true, QStringLiteral("Ada"));
    wrong["protocol"] = 9;
    bridge_->inbox.append(wrong);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bridge_->last("warning").has_value(); }));
    const Person bob;
    bridge_->posted.clear();
    bridge_->inbox.append(openMessage(bob, board_, true, QStringLiteral("Bob")));
    ASSERT_TRUE(QTest::qWaitFor([&] { return bridge_->last("warning").has_value(); }));
    EXPECT_TRUE((*bridge_->last("warning"))["text"].toString().contains("not a member"));
    bridge_->inbox.append({{"type", "frobnicate"}});
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return (*bridge_->last("warning"))["text"].toString().contains("frobnicate"); }));
}

} // namespace
} // namespace peerchalk::hosted
