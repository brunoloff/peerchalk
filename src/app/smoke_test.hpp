#pragma once

namespace peerchalk {

// For automated checks of release builds: when PEERCHALK_SMOKE_TEST names an image file, the app
// waits for its window, saves a screenshot there, prints "PeerChalk started" and quits with
// status 0, or quits with status 2 if no window appeared. Does nothing otherwise.
void startSmokeTestIfRequested();

} // namespace peerchalk
