// End-to-end sync between PeerChalk users over p2pdc, on a private OpenDHT network
// running on the loopback interface.
#include "sync/network.hpp"
#include "ui/library.hpp"

#include <opendht.h>

#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

#include <future>
#include <memory>

namespace peerchalk {
namespace {

using namespace std::chrono_literals;

constexpr std::uint32_t networkId = 0x50434b31; // private to these tests
constexpr int timeoutMs = 90000;

// Two bootstrap nodes forming a private DHT on 127.0.0.1.
class LocalDht {
  public:
    LocalDht() {
        dht::DhtRunner::Config config;
        config.dht_config.node_config.network = networkId;
        config.peer_discovery = false;
        config.peer_publish = false;
        first_.run("127.0.0.1", "::1", "0", config);
        second_.run("127.0.0.1", "::1", "0", config);
        auto done = std::make_shared<std::promise<bool>>();
        auto result = done->get_future();
        second_.bootstrap(first_.getBound(), [done](bool ok) { done->set_value(ok); });
        bootstrapped_ = result.wait_for(10s) == std::future_status::ready && result.get();
    }
    ~LocalDht() {
        first_.join();
        second_.join();
    }
    [[nodiscard]] bool ready() const { return bootstrapped_; }
    [[nodiscard]] std::uint16_t port() { return first_.getBoundPort(AF_INET); }

  private:
    dht::DhtRunner first_;
    dht::DhtRunner second_;
    bool bootstrapped_{false};
};

// One PeerChalk installation with networking on the local DHT.
struct Device {
    explicit Device(LocalDht &dht, const QString &transferCode = {}) : library(dir.path()) {
        if (!transferCode.isEmpty()) {
            EXPECT_TRUE(library.adoptIdentity(transferCode));
        }
        sync::NetworkConfig config;
        config.bootstrap = {{"127.0.0.1", dht.port()}};
        config.networkId = networkId;
        config.loopbackOnly = true;
        network = std::make_unique<sync::Network>(library, config);
        network->start();
    }
    ~Device() { network->shutdown(3s); }

    [[nodiscard]] std::size_t strokesOn(const QString &board) {
        const QUuid id = QUuid::fromString(board);
        if (!library.workspace().hasBoard(id) || !library.workspace().board(id).manifest)
            return 0;
        return library.document(id).board().strokes().size();
    }
    void draw(const QString &board, float x) {
        Stroke stroke(0xFF000000U, 2.0F);
        stroke.addPoint({x, 0.0F, 1.0F});
        stroke.addPoint({x + 10.0F, 5.0F, 1.0F});
        library.document(QUuid::fromString(board)).board().addStroke(stroke);
    }

    QTemporaryDir dir;
    Library library;
    std::unique_ptr<sync::Network> network;
};

class NetworkTest : public ::testing::Test {
  protected:
    void SetUp() override { ASSERT_TRUE(dht_.ready()) << "the local DHT did not start"; }
    LocalDht dht_;
};

TEST_F(NetworkTest, ProjectMembersShareBoardsLive) {
    Device alice(dht_);
    Device bob(dht_);
    ASSERT_TRUE(alice.library.addContact(bob.library.contactCode()));
    const QString project = alice.library.createProject(QStringLiteral("Thesis"));
    alice.library.setProjectMember(project, bob.library.userKey(), true);
    const QString board = alice.library.createBoard(QStringLiteral("Sketch"));
    alice.draw(board, 0.0F);
    ASSERT_FALSE(bob.library.joinInvite(alice.library.projectInvite(project)).isEmpty());

    ASSERT_TRUE(QTest::qWaitFor([&] { return bob.strokesOn(board) == 1; }, timeoutMs))
        << "Bob never received Alice's board";
    EXPECT_EQ(bob.library.workspace().board(QUuid::fromString(board)).name(),
              QStringLiteral("Sketch"));

    bob.draw(board, 50.0F);
    ASSERT_TRUE(QTest::qWaitFor([&] { return alice.strokesOn(board) == 2; }, timeoutMs))
        << "Alice never received Bob's stroke";
    alice.draw(board, 100.0F);
    EXPECT_TRUE(QTest::qWaitFor([&] { return bob.strokesOn(board) == 3; }, timeoutMs));
}

TEST_F(NetworkTest, InviteWithoutMembershipRevealsNoContent) {
    Device alice(dht_);
    Device eve(dht_);
    const QString project = alice.library.createProject(QStringLiteral("Private"));
    const QString board = alice.library.createBoard(QStringLiteral("Secret"));
    alice.draw(board, 0.0F);
    ASSERT_FALSE(eve.library.joinInvite(alice.library.projectInvite(project)).isEmpty());
    // Eve reaches Alice and learns the project's manifest, but she is not a member.
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            return eve.library.workspace().project(QUuid::fromString(project)).manifest.has_value();
        },
        timeoutMs));
    QTest::qWait(3000);
    EXPECT_FALSE(eve.library.workspace().hasBoard(QUuid::fromString(board)));
}

// The everyday flow: the admin sends only the invite; the newcomer shows up as a join
// request, and approving it delivers the boards.
TEST_F(NetworkTest, ApprovingAJoinRequestSharesTheBoards) {
    Device alice(dht_);
    Device bob(dht_);
    const QString project = alice.library.createProject(QStringLiteral("Thesis"));
    const QString board = alice.library.createBoard(QStringLiteral("Sketch"));
    alice.draw(board, 0.0F);
    ASSERT_FALSE(bob.library.joinInvite(alice.library.projectInvite(project)).isEmpty());
    ASSERT_TRUE(QTest::qWaitFor([&] { return alice.network->joinRequests(project).size() == 1; },
                                timeoutMs))
        << "Alice never saw Bob's request";
    EXPECT_TRUE(bob.library.currentProjectPending());
    EXPECT_EQ(bob.library.boards()->rowCount(), 0);
    const QVariantMap request = alice.network->joinRequests(project).front().toMap();
    EXPECT_EQ(request["key"].toString(), bob.library.userKey());
    alice.library.admitToProject(project, request["key"].toString(), request["name"].toString());
    ASSERT_TRUE(QTest::qWaitFor([&] { return bob.strokesOn(board) == 1; }, timeoutMs))
        << "Bob never received the board after being approved";
    EXPECT_FALSE(bob.library.currentProjectPending());
    EXPECT_TRUE(alice.network->joinRequests(project).isEmpty());
}

TEST_F(NetworkTest, NameAndColourChangesReachConnectedPeers) {
    Device alice(dht_);
    Device bob(dht_);
    ASSERT_TRUE(alice.library.addContact(bob.library.contactCode()));
    const QString project = alice.library.createProject(QStringLiteral("Thesis"));
    alice.library.setProjectMember(project, bob.library.userKey(), true);
    const QString board = alice.library.createBoard(QStringLiteral("Sketch"));
    bob.library.joinInvite(alice.library.projectInvite(project));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bob.library.workspace().hasBoard(QUuid::fromString(board)); }, timeoutMs));
    bob.network->setPresence(board, QPointF(1, 2), true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return alice.network->boardPresence(board).size() == 1; },
                                timeoutMs));
    bob.library.setDisplayName(QStringLiteral("Bruno 1"));
    bob.library.setColorIndex(3);
    EXPECT_TRUE(QTest::qWaitFor(
        [&] {
            const QVariantList people = alice.network->boardPresence(board);
            return !people.isEmpty() &&
                   people.front().toMap()["name"].toString() == QStringLiteral("Bruno 1") &&
                   people.front().toMap()["colorIndex"].toInt() == 3;
        },
        timeoutMs));
}

TEST_F(NetworkTest, ABoardInviteSharesOnlyThatBoard) {
    Device alice(dht_);
    Device carol(dht_);
    ASSERT_TRUE(alice.library.addContact(carol.library.contactCode()));
    alice.library.createProject(QStringLiteral("Thesis"));
    const QString shared = alice.library.createBoard(QStringLiteral("For Carol"));
    const QString other = alice.library.createBoard(QStringLiteral("Not for Carol"));
    alice.draw(shared, 0.0F);
    alice.draw(other, 0.0F);
    alice.library.setBoardMember(shared, carol.library.userKey(), true);
    ASSERT_FALSE(carol.library.joinInvite(alice.library.boardInvite(shared)).isEmpty());
    ASSERT_TRUE(QTest::qWaitFor([&] { return carol.strokesOn(shared) == 1; }, timeoutMs));
    EXPECT_FALSE(carol.library.workspace().hasBoard(QUuid::fromString(other)));
    EXPECT_EQ(carol.library.workspace().sharedBoards().size(), 1U);
}

TEST_F(NetworkTest, ALinkedDeviceGetsEverything) {
    Device phone(dht_);
    const QString project = phone.library.createProject(QStringLiteral("Mine"));
    const QString board = phone.library.createBoard(QStringLiteral("Notes"));
    phone.draw(board, 0.0F);
    Device laptop(dht_, phone.library.identityTransferCode());
    ASSERT_TRUE(QTest::qWaitFor([&] { return laptop.strokesOn(board) == 1; }, timeoutMs))
        << "the linked device never received the board";
    EXPECT_TRUE(laptop.library.isProjectAdmin(project));
    EXPECT_FALSE(laptop.library.projectInvite(project).isEmpty());
}

} // namespace
} // namespace peerchalk
