#pragma once

namespace peerchalk {

// Uses the Qt Quick Controls "Basic" style, which PeerChalk's controls restyle. Platform
// styles (such as KDE's Breeze) do not support that restyling. Call before creating the
// QML engine.
void useAppControlsStyle();

} // namespace peerchalk
