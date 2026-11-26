#include "sync/board_diagnosis.hpp"

#include <gtest/gtest.h>

namespace peerchalk::sync {
namespace {

using enum BoardConnectionState;

BoardConnectionFacts onlineWithServers() {
    BoardConnectionFacts facts;
    facts.online = true;
    facts.discoveryWorking = 2;
    facts.announced = true;
    return facts;
}

TEST(BoardDiagnosis, OfflineComesFirst) {
    BoardConnectionFacts facts = onlineWithServers();
    facts.online = false;
    facts.direct = 3;
    EXPECT_EQ(diagnose(facts), Offline);
}

TEST(BoardDiagnosis, AConnectionOutweighsServerProblems) {
    BoardConnectionFacts facts;
    facts.online = true;
    facts.discoveryUnreachable = 4;
    facts.relayed = 1;
    EXPECT_EQ(diagnose(facts), Relayed);
    facts.direct = 1;
    EXPECT_EQ(diagnose(facts), Connected);
}

TEST(BoardDiagnosis, AConnectionIsStuckOnlyAfterAWhile) {
    BoardConnectionFacts facts = onlineWithServers();
    facts.connecting = 1;
    facts.longestConnectingMs = stuckAfterMs - 1;
    EXPECT_EQ(diagnose(facts), Connecting);
    facts.longestConnectingMs = stuckAfterMs;
    EXPECT_EQ(diagnose(facts), Stuck);
}

TEST(BoardDiagnosis, NoDiscoveryOnlyOnceEveryCheckFailed) {
    BoardConnectionFacts facts;
    facts.online = true;
    facts.discoveryUnreachable = 2;
    facts.discoveryChecking = 1;
    EXPECT_EQ(diagnose(facts), Checking);
    facts.discoveryChecking = 0;
    EXPECT_EQ(diagnose(facts), NoDiscovery);
    facts.discoveryUnreachable = 0; // nothing checked at all
    EXPECT_EQ(diagnose(facts), Checking);
}

TEST(BoardDiagnosis, AloneOnceAnnounced) {
    BoardConnectionFacts facts = onlineWithServers();
    facts.announced = false;
    EXPECT_EQ(diagnose(facts), Announcing);
    facts.announced = true;
    EXPECT_EQ(diagnose(facts), Alone);
}

TEST(BoardDiagnosis, ANewerVersionExplainsBeingAlone) {
    BoardConnectionFacts facts = onlineWithServers();
    facts.updateNeeded = true;
    EXPECT_EQ(diagnose(facts), UpdateNeeded);
    facts.direct = 1; // someone else here still talks to us
    EXPECT_EQ(diagnose(facts), Connected);
}

TEST(BoardDiagnosis, EveryStateHasAName) {
    for (const BoardConnectionState state : {Offline, Connected, Relayed, UpdateNeeded, Stuck,
                                             Connecting, NoDiscovery, Checking, Announcing, Alone})
        EXPECT_FALSE(stateName(state).isEmpty());
    EXPECT_EQ(stateName(NoDiscovery), QStringLiteral("noDiscovery"));
}

} // namespace
} // namespace peerchalk::sync
