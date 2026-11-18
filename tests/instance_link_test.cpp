#include "library/instance_slot.hpp"
#include "sync/instance_link.hpp"
#include "sync/network.hpp"
#include "ui/library.hpp"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

namespace peerchalk::sync {
namespace {

constexpr int waitMs = 5000;

// One running app instance with networking off, linked to the others in `location`.
struct Instance {
    explicit Instance(const QString &location, const QString &transferCode = {})
        : slot(InstanceSlot::claim(location)), link(InstanceLink::create(*slot)),
          library(slot->root()) {
        if (!transferCode.isEmpty()) {
            EXPECT_TRUE(library.adoptIdentity(transferCode));
        }
        NetworkConfig config;
        config.enabled = false;
        network = std::make_unique<Network>(library, config, nullptr, link.get());
    }
    [[nodiscard]] std::size_t strokesOn(const QString &board) {
        const QUuid id = QUuid::fromString(board);
        return library.workspace().hasBoard(id) ? library.document(id).board().strokes().size() : 0;
    }
    void draw(const QString &board, float x) {
        Stroke stroke(0xFF000000U, 2.0F);
        stroke.addPoint({x, 0.0F, 1.0F});
        stroke.addPoint({x + 5.0F, 5.0F, 1.0F});
        library.document(QUuid::fromString(board)).board().addStroke(stroke);
    }
    std::unique_ptr<InstanceSlot> slot;
    std::unique_ptr<InstanceLink> link;
    Library library;
    std::unique_ptr<Network> network;
};

TEST(InstanceLink, DeliversWholeMessagesInBothDirections) {
    QTemporaryDir location;
    auto firstSlot = InstanceSlot::claim(location.path());
    auto secondSlot = InstanceSlot::claim(location.path());
    auto first = InstanceLink::create(*firstSlot);
    QSignalSpy joined(first.get(), &InstanceLink::instanceJoined);
    auto second = InstanceLink::create(*secondSlot);
    ASSERT_TRUE(QTest::qWaitFor([&] { return joined.size() == 1; }, waitMs));
    QSignalSpy atFirst(first.get(), &InstanceLink::received);
    QSignalSpy atSecond(second.get(), &InstanceLink::received);
    const QByteArray large(3 * 1024 * 1024, 'x');
    first->send(large);
    second->send("reply");
    ASSERT_TRUE(QTest::qWaitFor([&] { return atSecond.size() == 1; }, waitMs));
    ASSERT_TRUE(QTest::qWaitFor([&] { return atFirst.size() == 1; }, waitMs));
    EXPECT_EQ(atSecond.front().front().toByteArray(), large);
    EXPECT_EQ(atFirst.front().front().toByteArray(), QByteArray("reply"));
}

TEST(InstanceLink, InstancesShareTheSameBoardWithoutTheNetwork) {
    QTemporaryDir location;
    Instance first(location.path());
    Instance second(location.path(), first.library.identityTransferCode());
    first.library.createProject(QStringLiteral("Project"));
    const QString board = first.library.createBoard(QStringLiteral("Board"));
    first.draw(board, 0.0F);
    ASSERT_TRUE(QTest::qWaitFor([&] { return second.strokesOn(board) == 1; }, waitMs));
    second.draw(board, 20.0F);
    EXPECT_TRUE(QTest::qWaitFor([&] { return first.strokesOn(board) == 2; }, waitMs));
    first.library.setDisplayName(QStringLiteral("Renamed"));
    EXPECT_TRUE(QTest::qWaitFor(
        [&] { return second.library.displayName() == QStringLiteral("Renamed"); }, waitMs));
}

TEST(InstanceLink, InstancesOfDifferentAccountsStayApart) {
    QTemporaryDir location;
    Instance first(location.path());
    Instance stranger(location.path());
    first.library.createProject(QStringLiteral("Project"));
    const QString board = first.library.createBoard(QStringLiteral("Board"));
    first.draw(board, 0.0F);
    QTest::qWait(300);
    EXPECT_EQ(stranger.strokesOn(board), 0U);
    EXPECT_TRUE(stranger.library.workspace().projects().empty());
}

} // namespace
} // namespace peerchalk::sync
