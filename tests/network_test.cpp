// End-to-end sync between PeerChalk users over p2pdc, on a private OpenDHT network
// running on the loopback interface.
#include "board/board_pages.hpp"
#include "sync/network.hpp"
#include "test_pdf.hpp"
#include "ui/library.hpp"

#include <opendht.h>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

#include <functional>
#include <future>
#include <memory>

namespace peerchalk {
namespace {

using namespace std::chrono_literals;

constexpr std::uint32_t networkId = 0x50434b31; // private to these tests
constexpr int timeoutMs = 90000;
constexpr qsizetype maxBoardMessageBytes = 1024 * 1024;

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
    explicit Device(LocalDht &dht, const QString &transferCode = {},
                    const std::function<void(Library &)> &prepare = {})
        : library(dir.path()) {
        if (!transferCode.isEmpty()) {
            EXPECT_TRUE(library.adoptIdentity(transferCode));
        }
        if (prepare)
            prepare(library);
        config.bootstrap = {{"127.0.0.1", dht.port()}};
        config.networkId = networkId;
        config.loopbackOnly = true;
        goOnline();
    }
    ~Device() { goOffline(); }

    void goOnline() {
        network = std::make_unique<sync::Network>(library, config);
        network->start();
    }
    void goOffline() {
        if (!network)
            return; // already offline
        network->shutdown(3s);
        network.reset();
    }

    [[nodiscard]] std::size_t strokesOn(const QString &board) {
        const QUuid id = QUuid::fromString(board);
        const Workspace &workspace = library.workspace();
        if (!workspace.hasBoard(id) || !workspace.mayAccessBoard(workspace.user(), id))
            return 0;
        return library.document(id).board().strokes().size();
    }
    void draw(const QString &board, float x) {
        Stroke stroke(0xFF000000U, 2.0F);
        stroke.addPoint({x, 0.0F, 1.0F});
        stroke.addPoint({x + 10.0F, 5.0F, 1.0F});
        library.document(QUuid::fromString(board)).board().addStroke(stroke);
    }

    void drawLongStrokes(const QString &board, int strokes, int points) {
        auto &document = library.document(QUuid::fromString(board)).board();
        for (int s = 0; s < strokes; ++s) {
            Stroke stroke(0xFF000000U, 2.0F);
            for (int p = 0; p < points; ++p)
                stroke.addPoint({static_cast<float>(s) + static_cast<float>(p) * 0.37F,
                                 static_cast<float>(p * p % 9973) * 0.11F, 1.0F});
            document.addStroke(stroke);
        }
    }

    QTemporaryDir dir;
    sync::NetworkConfig config;
    Library library;
    std::unique_ptr<sync::Network> network;
};

class NetworkTest : public ::testing::Test {
  protected:
    void SetUp() override { ASSERT_TRUE(dht_.ready()) << "the local DHT did not start"; }
    LocalDht dht_;
};

// Alice and Bob in one project with one board, connected and in step.
struct TwoMembers {
    explicit TwoMembers(LocalDht &dht) : alice(dht), bob(dht) {
        EXPECT_TRUE(alice.library.addContact(bob.library.contactCode()));
        project = alice.library.createProject(QStringLiteral("Thesis"));
        alice.library.setProjectRole(project, bob.library.userKey(), QStringLiteral("member"));
        board = alice.library.createBoard(QStringLiteral("Sketch"));
        EXPECT_FALSE(bob.library.joinInvite(alice.library.projectInvite(project)).isEmpty());
    }
    Device alice;
    Device bob;
    QString project;
    QString board;
};

TEST_F(NetworkTest, BoardsTooLargeForOneMessageStillArrive) {
    TwoMembers shared(dht_);
    shared.alice.drawLongStrokes(shared.board, 150, 2000);
    shared.alice.library.saveBoards();
    ASSERT_GT(shared.alice.library.boardState(shared.board).size(), 2 * maxBoardMessageBytes);
    EXPECT_TRUE(
        QTest::qWaitFor([&] { return shared.bob.strokesOn(shared.board) == 150; }, timeoutMs))
        << "the large board never arrived";
}

QByteArray bytesOf(const Bytes &bytes) {
    return {reinterpret_cast<const char *>(bytes.data()), qsizetype(bytes.size())};
}

std::string firstPdfOf(Library &library, const QString &board) {
    return library.document(QUuid::fromString(board))
        .board()
        .pages()
        .columns()[1]
        .items[0]
        .pdf->blob;
}

TEST_F(NetworkTest, PageBoardsArriveWithTheirPdfs) {
    TwoMembers shared(dht_);
    QSignalSpy progress(&shared.bob.library, &Library::blobProgressChanged);
    const QString board = shared.alice.library.createPageBoard("Reading", "A4", false, "blank");
    const Bytes pdf = test::samplePdf(700 * 1024); // several pieces
    ASSERT_TRUE(shared.alice.library.addPdf(
        board, QByteArray(reinterpret_cast<const char *>(pdf.data()), qsizetype(pdf.size())),
        "notes", {}));
    const std::string blob = shared.alice.library.document(QUuid::fromString(board))
                                 .board()
                                 .pages()
                                 .columns()[1]
                                 .items[0]
                                 .pdf->blob;
    EXPECT_TRUE(QTest::qWaitFor(
        [&] { return shared.bob.library.blobs().has(QString::fromStdString(blob)); }, timeoutMs))
        << "the PDF never arrived";
    EXPECT_EQ(shared.bob.library.document(QUuid::fromString(board)).board().pages().kind(),
              BoardKind::Pages);
    EXPECT_GE(progress.count(), 3); // it said how far it had got, piece by piece
    EXPECT_TRUE(shared.bob.library.blobProgress(QString::fromStdString(blob)).isEmpty());
}

// Bob starts fetching a PDF; when the owner he fetches it from leaves, another owner who has
// it sends the rest. (Boards, and their PDFs, travel only to and from owners.)
TEST_F(NetworkTest, APdfComesFromSomeoneElseWhenItsSenderLeaves) {
    TwoMembers shared(dht_);
    Device carol(dht_);
    shared.alice.library.setDisplayName("Alice");
    carol.library.setDisplayName("Carol");
    ASSERT_TRUE(shared.alice.library.addContact(carol.library.contactCode()));
    shared.alice.library.setProjectRole(shared.project, carol.library.userKey(), "owner");
    ASSERT_FALSE(
        carol.library.joinInvite(shared.alice.library.projectInvite(shared.project)).isEmpty());
    shared.bob.goOffline();
    const QString board = shared.alice.library.createPageBoard("Reading", "A4", false, "blank");
    ASSERT_TRUE(shared.alice.library.addPdf(board, bytesOf(test::samplePdf(8 << 20)), "big", {}));
    const QString blob = QString::fromStdString(firstPdfOf(shared.alice.library, board));
    ASSERT_TRUE(QTest::qWaitFor([&] { return carol.library.blobs().has(blob); }, timeoutMs));
    shared.bob.goOnline();
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return shared.bob.library.blobProgress(blob).value("percent").toInt() > 0; },
        timeoutMs));
    const QString sender = shared.bob.library.blobProgress(blob).value("from").toString();
    (sender == "Carol" ? carol : shared.alice).goOffline();
    EXPECT_TRUE(QTest::qWaitFor([&] { return shared.bob.library.blobs().has(blob); }, timeoutMs))
        << "the rest of the PDF never came from the one still here";
}

TEST_F(NetworkTest, EditsMadeWhileApartMergeAfterReconnecting) {
    TwoMembers shared(dht_);
    shared.alice.draw(shared.board, 0.0F);
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return shared.bob.strokesOn(shared.board) == 1; }, timeoutMs));
    shared.bob.goOffline();
    shared.alice.draw(shared.board, 20.0F);
    shared.bob.draw(shared.board, 40.0F);
    shared.bob.goOnline();
    EXPECT_TRUE(
        QTest::qWaitFor([&] { return shared.bob.strokesOn(shared.board) == 3; }, timeoutMs));
    EXPECT_TRUE(
        QTest::qWaitFor([&] { return shared.alice.strokesOn(shared.board) == 3; }, timeoutMs));
}

TEST_F(NetworkTest, TheProjectOnScreenConnectsEvenWithTooManyProjects) {
    QString project;
    Device bob(dht_);
    Device alice(dht_, {}, [&](Library &library) {
        EXPECT_TRUE(library.addContact(bob.library.contactCode()));
        project = library.createProject(QStringLiteral("Shared"));
        library.setProjectRole(project, bob.library.userKey(), QStringLiteral("member"));
        for (int i = 0; i < 20; ++i)
            library.createProject(QStringLiteral("Other %1").arg(i));
        library.setCurrentProjectId(project);
    });
    const QString board = [&] {
        const QString id = alice.library.createBoard(QStringLiteral("Sketch"));
        alice.draw(id, 0.0F);
        return id;
    }();
    ASSERT_FALSE(bob.library.joinInvite(alice.library.projectInvite(project)).isEmpty());
    EXPECT_TRUE(QTest::qWaitFor([&] { return bob.strokesOn(board) == 1; }, timeoutMs));
}

TEST_F(NetworkTest, StoppingWithoutWaitingReportsWhenDone) {
    Device alice(dht_);
    ASSERT_TRUE(QTest::qWaitFor([&] { return alice.network->online(); }, timeoutMs));
    bool done = false;
    alice.network->shutdownThen(3s, [&] { done = true; });
    EXPECT_FALSE(done); // never from inside the call
    ASSERT_TRUE(QTest::qWaitFor([&] { return done; }, 5000));
    EXPECT_FALSE(alice.network->online());
}

TEST(NetworkShutdownTest, ReportsDoneWhenNothingWasRunning) {
    QTemporaryDir dir;
    Library library(dir.path());
    sync::NetworkConfig config;
    config.enabled = false;
    sync::Network network(library, config);
    bool done = false;
    network.shutdownThen(3s, [&] { done = true; });
    EXPECT_TRUE(QTest::qWaitFor([&] { return done; }, 1000));
}

TEST_F(NetworkTest, ProjectMembersShareBoardsLive) {
    Device alice(dht_);
    Device bob(dht_);
    ASSERT_TRUE(alice.library.addContact(bob.library.contactCode()));
    const QString project = alice.library.createProject(QStringLiteral("Thesis"));
    alice.library.setProjectRole(project, bob.library.userKey(), QStringLiteral("member"));
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
    // Eve reaches Alice and learns who is in the project, but she is not a member.
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return eve.library.workspace().project(QUuid::fromString(project)).state.exists; },
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
    alice.library.setProjectRole(project, bob.library.userKey(), QStringLiteral("member"));
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

TEST_F(NetworkTest, PeopleOnABoardShareWhatTheyAreLookingAt) {
    TwoMembers shared(dht_);
    const QRectF view(-50.0, 25.0, 640.0, 480.0);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return shared.bob.library.workspace().hasBoard(QUuid::fromString(shared.board)); },
        timeoutMs));
    shared.bob.network->setPresence(shared.board, QPointF(1, 2), true);
    shared.bob.network->setPresenceView(shared.board, view);
    EXPECT_TRUE(QTest::qWaitFor(
        [&] {
            const QVariantList people = shared.alice.network->boardPresence(shared.board);
            return !people.isEmpty() && people.front().toMap()["view"].toRectF() == view;
        },
        timeoutMs));
}

// Waits until `approver` sees someone asking to join the item and returns that request.
QVariantMap awaitRequest(Device &approver, const QString &item) {
    if (!QTest::qWaitFor([&] { return !approver.network->joinRequests(item).isEmpty(); },
                         timeoutMs))
        return {};
    return approver.network->joinRequests(item).front().toMap();
}

TEST_F(NetworkTest, ABoardInviteSharesOnlyThatBoardOnceApproved) {
    Device alice(dht_);
    Device carol(dht_);
    alice.library.createProject(QStringLiteral("Thesis"));
    const QString shared = alice.library.createBoard(QStringLiteral("For Carol"));
    const QString other = alice.library.createBoard(QStringLiteral("Not for Carol"));
    alice.draw(shared, 0.0F);
    alice.draw(other, 0.0F);
    ASSERT_FALSE(carol.library.joinInvite(alice.library.boardInvite(shared)).isEmpty());
    const QVariantMap request = awaitRequest(alice, shared);
    ASSERT_EQ(request["key"].toString(), carol.library.userKey()) << "no request from Carol";
    EXPECT_EQ(carol.strokesOn(shared), 0U);
    alice.library.admitToBoard(shared, request["key"].toString(), request["name"].toString());
    ASSERT_TRUE(QTest::qWaitFor([&] { return carol.strokesOn(shared) == 1; }, timeoutMs));
    EXPECT_FALSE(carol.library.workspace().hasBoard(QUuid::fromString(other)));
    EXPECT_EQ(carol.library.workspace().sharedBoards().size(), 1U);
}

// An owner who is not the admin approves a newcomer, and the admin is not needed.
TEST_F(NetworkTest, AnOwnerApprovesWhileTheAdminIsOffline) {
    auto alice = std::make_unique<Device>(dht_);
    Device bob(dht_);
    Device dave(dht_);
    ASSERT_TRUE(alice->library.addContact(bob.library.contactCode()));
    const QString project = alice->library.createProject(QStringLiteral("Thesis"));
    const QString board = alice->library.createBoard(QStringLiteral("Sketch"));
    alice->draw(board, 0.0F);
    alice->library.setProjectRole(project, bob.library.userKey(), QStringLiteral("owner"));
    bob.library.joinInvite(alice->library.projectInvite(project));
    ASSERT_TRUE(QTest::qWaitFor([&] { return bob.strokesOn(board) == 1; }, timeoutMs));
    alice.reset(); // the admin goes offline
    ASSERT_FALSE(dave.library.joinInvite(bob.library.projectInvite(project)).isEmpty());
    const QVariantMap request = awaitRequest(bob, project);
    ASSERT_EQ(request["key"].toString(), dave.library.userKey()) << "Bob never saw Dave";
    bob.library.admitToProject(project, request["key"].toString(), request["name"].toString());
    ASSERT_TRUE(QTest::qWaitFor([&] { return dave.strokesOn(board) == 1; }, timeoutMs))
        << "Dave never received the board from Bob";
    EXPECT_EQ(dave.library.projectRole(project), QStringLiteral("member"));
}

// Run by hand against a browser (--gtest_also_run_disabled_tests): an owner on the public
// network writes a board link to the file named by PEERCHALK_INTEROP_LINK, waits for the
// visitor's stroke and answers with one of its own.
TEST(BrowserInterop, DISABLED_OwnerServesALink) {
    QTemporaryDir dir;
    Library library(dir.path());
    sync::Network network(library, sync::NetworkConfig::from({}));
    network.start();
    library.createProject(QStringLiteral("Interop"));
    const QString id = library.createBoard(QStringLiteral("Desktop board"));
    auto &board = library.document(QUuid::fromString(id)).board();
    const auto draw = [&](float y) {
        Stroke stroke(0xFF000000U, 3.0F);
        stroke.addPoint({120.0F, y, 1.0F});
        stroke.addPoint({420.0F, y, 1.0F});
        board.addStroke(stroke);
    };
    draw(200.0F);
    QFile file(qEnvironmentVariable("PEERCHALK_INTEROP_LINK"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(library.createBoardLink(id, qEnvironmentVariable("PEERCHALK_SITE_URL")).toUtf8());
    file.close();
    ASSERT_TRUE(QTest::qWaitFor([&] { return board.strokes().size() >= 2; }, 300000))
        << "no stroke arrived from the browser";
    draw(320.0F);
    QTest::qWait(45000); // time for the browser to receive it
    network.shutdown(3s);
}

TEST_F(NetworkTest, ABoardLinkAdmitsItsHolderWhileAnOwnerIsOnline) {
    Device alice(dht_);
    Device bob(dht_);
    alice.library.createProject(QStringLiteral("Thesis"));
    const QString board = alice.library.createBoard(QStringLiteral("Sketch"));
    alice.draw(board, 0.0F);
    const QString link = alice.library.createBoardLink(board, QStringLiteral("https://x.example"));
    ASSERT_FALSE(link.isEmpty());
    ASSERT_EQ(bob.library.openBoardLink(link), board);
    EXPECT_EQ(bob.network->boardAccess(board), QStringLiteral("waiting"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return bob.strokesOn(board) == 1; }, timeoutMs));
    EXPECT_EQ(bob.library.boardRole(board), QStringLiteral("member"));
    EXPECT_TRUE(QTest::qWaitFor(
        [&] { return bob.network->boardAccess(board) == QStringLiteral("edit"); }, timeoutMs));
}

TEST_F(NetworkTest, MembersExchangeBoardsOnlyWhileAnOwnerIsConnected) {
    auto alice = std::make_unique<Device>(dht_);
    Device bob(dht_);
    Device carol(dht_);
    ASSERT_TRUE(alice->library.addContact(bob.library.contactCode()));
    ASSERT_TRUE(alice->library.addContact(carol.library.contactCode()));
    const QString project = alice->library.createProject(QStringLiteral("Thesis"));
    const QString board = alice->library.createBoard(QStringLiteral("Sketch"));
    alice->draw(board, 0.0F);
    alice->library.setProjectRole(project, bob.library.userKey(), QStringLiteral("member"));
    alice->library.setProjectRole(project, carol.library.userKey(), QStringLiteral("member"));
    const QString invite = alice->library.projectInvite(project);
    bob.library.joinInvite(invite);
    carol.library.joinInvite(invite);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bob.strokesOn(board) == 1; }, timeoutMs));
    ASSERT_TRUE(QTest::qWaitFor([&] { return carol.strokesOn(board) == 1; }, timeoutMs));
    alice->goOffline();
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bob.network->boardAccess(board) == QStringLiteral("offline"); }, timeoutMs));
    bob.draw(board, 20.0F); // what a modified app could do; the editor itself refuses
    QTest::qWait(3000);
    EXPECT_EQ(carol.strokesOn(board), 1U);
    alice->goOnline();
    EXPECT_TRUE(QTest::qWaitFor([&] { return carol.strokesOn(board) == 2; }, timeoutMs));
}

// The admin's signed changes travel through anyone who has them.
TEST_F(NetworkTest, MembersPassOnTheAdminsChanges) {
    auto alice = std::make_unique<Device>(dht_);
    Device bob(dht_);
    Device carol(dht_);
    ASSERT_TRUE(alice->library.addContact(bob.library.contactCode()));
    ASSERT_TRUE(alice->library.addContact(carol.library.contactCode()));
    const QString project = alice->library.createProject(QStringLiteral("Thesis"));
    const QString board = alice->library.createBoard(QStringLiteral("Sketch"));
    alice->draw(board, 0.0F);
    alice->library.setProjectRole(project, bob.library.userKey(), QStringLiteral("member"));
    alice->library.setProjectRole(project, carol.library.userKey(), QStringLiteral("member"));
    const QString invite = alice->library.projectInvite(project);
    bob.library.joinInvite(invite);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bob.strokesOn(board) == 1; }, timeoutMs));
    alice.reset(); // Carol only ever meets Bob
    carol.library.joinInvite(invite);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return carol.library.projectRole(project) == QStringLiteral("member"); }, timeoutMs))
        << "Carol never received her role through Bob";
    EXPECT_EQ(carol.strokesOn(board), 0U); // contents need an owner
}

Stroke unfinishedStroke() {
    Stroke stroke(0xFF000000U, 2.0F);
    stroke.addPoint({1.0F, 1.0F, 1.0F});
    stroke.addPoint({2.0F, 2.0F, 1.0F});
    return stroke;
}

// Both people have the board open and are connected.
struct SharedBoard {
    SharedBoard(LocalDht &dht) : alice(dht), bob(dht) {
        EXPECT_TRUE(alice.library.addContact(bob.library.contactCode()));
        project = alice.library.createProject(QStringLiteral("Thesis"));
        alice.library.setProjectRole(project, bob.library.userKey(), QStringLiteral("member"));
        board = alice.library.createBoard(QStringLiteral("Sketch"));
        alice.draw(board, 0.0F);
        bob.library.joinInvite(alice.library.projectInvite(project));
        ready = QTest::qWaitFor([&] { return bob.strokesOn(board) == 1; }, timeoutMs);
        alice.network->setPresence(board, QPointF(), true);
        bob.network->setPresence(board, QPointF(), true);
    }
    [[nodiscard]] BoardDocument &document(Device &device) const {
        return device.library.document(QUuid::fromString(board));
    }
    // Keeps reporting the draft, as drawing does, until Bob shows it.
    [[nodiscard]] bool bobSeesDraft(const Stroke &stroke) {
        return QTest::qWaitFor(
            [&] {
                document(alice).publishDraft(stroke, DraftState::Drawing);
                return !document(bob).remoteDrafts().empty();
            },
            timeoutMs);
    }
    Device alice;
    Device bob;
    QString project;
    QString board;
    bool ready{false};
};

TEST_F(NetworkTest, StrokesAppearWhileTheyAreDrawn) {
    SharedBoard shared(dht_);
    ASSERT_TRUE(shared.ready);
    const Stroke stroke = unfinishedStroke();
    ASSERT_TRUE(shared.bobSeesDraft(stroke)) << "Bob never saw the stroke being drawn";
    EXPECT_EQ(shared.document(shared.bob).remoteDrafts().begin()->second.points().size(), 2U);
    EXPECT_EQ(shared.bob.strokesOn(shared.board), 1U); // not part of the board yet
    shared.document(shared.alice).publishDraft(stroke, DraftState::Finished);
    shared.document(shared.alice).board().addStroke(stroke);
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return shared.bob.strokesOn(shared.board) == 2; }, timeoutMs));
    EXPECT_TRUE(QTest::qWaitFor([&] { return shared.document(shared.bob).remoteDrafts().empty(); },
                                timeoutMs))
        << "the preview should give way to the finished stroke";
}

TEST_F(NetworkTest, AnAbandonedStrokeDisappears) {
    SharedBoard shared(dht_);
    ASSERT_TRUE(shared.ready);
    const Stroke stroke = unfinishedStroke();
    ASSERT_TRUE(shared.bobSeesDraft(stroke));
    shared.document(shared.alice).publishDraft(stroke, DraftState::Cancelled);
    EXPECT_TRUE(QTest::qWaitFor([&] { return shared.document(shared.bob).remoteDrafts().empty(); },
                                timeoutMs));
    EXPECT_EQ(shared.bob.strokesOn(shared.board), 1U);
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
