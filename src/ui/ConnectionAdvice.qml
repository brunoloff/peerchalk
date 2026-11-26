pragma Singleton

import QtQuick

// What the board's network pane says about a connection state (Network.boardConnection):
// a tone, a headline, why, and what to try. Written for whoever is drawing, not for whoever
// runs the servers; the log is there for that.
QtObject {
    readonly property bool inBrowser: Qt.platform.os === "wasm"

    // "good", "busy" or "problem", for the colour of the dot.
    function tone(state) {
        return state === "connected" || state === "relayed" || state === "alone" ? "good"
             : state === "checking" || state === "announcing" || state === "connecting" ? "busy"
             : "problem";
    }

    function color(state) {
        const t = tone(state);
        return t === "good" ? Theme.working : t === "busy" ? Theme.accent : Theme.danger;
    }

    // The button's tooltip and the pane's headline.
    function title(state, facts) {
        switch (state) {
        case "connected": return facts.direct + facts.relayed === 1 ? qsTr("Connected to 1 device")
                               : qsTr("Connected to %1 devices").arg(facts.direct + facts.relayed);
        case "relayed": return qsTr("Connected through a relay");
        case "alone": return qsTr("Only you have this board open");
        case "checking": return qsTr("Checking the servers…");
        case "announcing": return qsTr("Announcing this board…");
        case "connecting": return qsTr("Connecting…");
        case "stuck": return qsTr("Others found, but no connection");
        case "noDiscovery": return qsTr("No server answers from this network");
        case "updateNeeded": return qsTr("Someone here uses a newer PeerChalk");
        default: return qsTr("Offline");
        }
    }

    function detail(state, facts) {
        switch (state) {
        case "connected":
            return facts.relayed > 0 ? qsTr("%1 directly, %2 through a relay. Edits and cursors go straight to them.")
                                           .arg(facts.direct).arg(facts.relayed)
                                     : qsTr("Directly. Edits and cursors go straight to them.");
        case "relayed":
            return qsTr("Your networks do not let you connect directly, so everything goes through a "
                + "relay server. It works; drawing may lag a little.");
        case "alone":
            return facts.seen > 0
                ? qsTr("PeerChalk sees %1 other device(s) announced here that are not connecting yet. "
                       + "Whoever opens the board connects to you; there is nothing to do on your side.").arg(facts.seen)
                : qsTr("Whoever opens it next connects to you; there is nothing to do on your side. "
                       + "Your edits are kept either way.");
        case "checking":
            return qsTr("PeerChalk finds the others through public discovery servers, and is checking "
                + "which of them answer.");
        case "announcing":
            return qsTr("The servers answer. PeerChalk is telling them that you have this board open, "
                + "which takes a few seconds.");
        case "connecting":
            return qsTr("PeerChalk found %1 device(s) on this board and is finding a way through both "
                + "of your networks.").arg(facts.connecting);
        case "stuck":
            return qsTr("PeerChalk found %1 device(s) on this board %2 seconds ago, but no connection "
                + "has got through. ").arg(facts.connecting).arg(facts.connectingSeconds)
                + (!facts.relayConfigured ? qsTr("No relay is set up, so both networks must allow a direct connection, and one of them does not.")
                 : !facts.relayWorking ? qsTr("The relay that gets through strict firewalls does not answer from here.")
                 : qsTr("Even the relay did not get through: the other side's network may block it."));
        case "noDiscovery":
            return qsTr("PeerChalk finds the others through %1 public servers, and none of them answers. "
                + "Something between this device and the internet blocks them.").arg(facts.discoveryUnreachable);
        case "updateNeeded":
            return qsTr("This version is too old to talk to them, so you do not see their edits, nor they yours.");
        default:
            return qsTr("PeerChalk is not connected to the network. Your edits are kept on this device "
                + "and go out once it is back.");
        }
    }

    // What the person can do, most likely to help first; empty when nothing is needed.
    function tips(state, facts) {
        const otherNetwork = qsTr("Try another network, such as a phone's hotspot: company, university "
            + "and hotel networks often block these connections.");
        const vpn = qsTr("Turn off a VPN, proxy or ad blocker, or allow this page in it, then reload.");
        const askThem = qsTr("Ask the others to open this pane too: the problem may be on their side.");
        switch (state) {
        case "stuck":
            return [otherNetwork, askThem, vpn, qsTr("Wait a minute: PeerChalk keeps trying.")];
        case "noDiscovery":
            return [qsTr("Check that this device is online: open any web page."), otherNetwork, vpn,
                    qsTr("With a firewall of your own, allow outgoing connections for PeerChalk.")];
        case "updateNeeded":
            return [inBrowser ? qsTr("Reload the page to get the newest PeerChalk; if it stays the same, "
                                     + "whoever runs this site needs to update it.")
                              : qsTr("Update PeerChalk, then open the board again.")];
        case "alone":
            return facts.seen > 0 ? [askThem] : [];
        case "offline":
            return [qsTr("Check that this device is online."),
                    inBrowser ? qsTr("Reload the page.") : qsTr("If networking is off in Settings → Network, turn it on.")];
        default:
            return [];
        }
    }

    // A member's line in the pane: {state, route, milliseconds} from boardConnection.
    function personLine(person) {
        if (person.you)
            return qsTr("You");
        const how = person.route === "relay" ? qsTr("through a relay") : qsTr("direct");
        const ms = typeof person.milliseconds === "number" ? " · " + qsTr("%1 ms").arg(person.milliseconds) : "";
        switch (person.state) {
        case "here": return qsTr("On this board · %1").arg(how) + ms;
        case "connected": return qsTr("Connected, on another board · %1").arg(how) + ms;
        case "connecting": return qsTr("Connecting…");
        default: return qsTr("Not connected");
        }
    }

    // The repository part: who commits the session's edits, and when they last were.
    function repositoryLine(repository) {
        const who = repository.scribe ? qsTr("This device commits everyone's edits to the repository, about every 30 seconds.")
            : repository.hasScribe && repository.scribeName.length > 0
                ? qsTr("%1's device commits everyone's edits to the repository.").arg(repository.scribeName)
                : qsTr("Nobody here can push now: edits are kept, and committed once someone who syncs opens the board.");
        const when = repository.pushedAt.length > 0
            ? " " + qsTr("Last commit at %1.").arg(new Date(repository.pushedAt).toLocaleTimeString(Qt.locale(), Locale.ShortFormat))
            : "";
        return who + when;
    }
}
