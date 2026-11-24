# PeerChalk

A peer-to-peer collaborative whiteboard, built on
[p2p-discovery-connect](https://github.com/brunoloff/p2pdc) for peer discovery and
authenticated WebRTC data channels.

Status: early development.

PeerChalk is licensed under the MIT License; see [LICENSE](LICENSE).
Third-party dependencies retain their respective licenses.

## Build

[Building PeerChalk](docs/BUILDING.md) covers a clean checkout, for Linux and for browsers.
In short, for Linux:

```sh
tools/setup.sh desktop
cmake --workflow --preset deps
cmake --workflow --preset linux
bin/peerchalk
```

For browsers:

```sh
tools/setup.sh web
deploy/web/build.sh
bin/peerchalk-web
```

Browsers find peers through discovery servers, all at once: by default Jami's OpenDHT proxy,
PeerChalk's own and two Nostr relays (Settings → Network → Discovery servers, one per line). Your identity and boards stay in that site's browser storage, and
each `--port` is a separate user. Any number of tabs and desktop windows can be open at
once, even on the same board: each is its own instance and they update each other directly.

## Sharing and access

Projects are collections of boards, and projects and boards each have an admin,
owners and members (an admin is also an owner, an owner also a member):

- The admin (at first the creator) makes people owners or members, changes or removes
  their role, renames or deletes the item, and can hand the admin role to someone else,
  becoming an owner.
- Owners send invites and add or remove members, but cannot change owners.
- Members of a project are members of every board in it; every member of a board can
  draw on it. Project owners add boards they own to the project; owners of both, or the
  project admin, take a board out again. A board can be in several projects.

An invite code only lets someone ask: when they open it they appear under Requests for
the owners, who approve them (or add people from their known people in advance). Every
change to who has access is signed by the person who made it, and peers replay these
changes and keep only those the signer's role allowed. Anyone who has them passes them
on, so the admin does not need to be online for new members to be let in. Having an
invite code alone gives no board contents.

A person (user key) can use several devices (device keys): "Link another device" under
Settings → People gives a secret code that a new installation pastes on its welcome screen.
Your devices then share your projects, boards and contacts automatically.

## Networking

PeerChalk finds peers through p2p-discovery-connect: every project, every board shared
on its own, and your own set of devices has a discovery secret, and devices holding it
find each other on the OpenDHT network and connect directly over WebRTC. Peers first
check that their protocol versions overlap and say what they have of each board, so only
missing changes travel; large ones are sent in parts. Boards sync as Yrs updates, live, with the other people's cursors and the strokes they are still drawing
shown on the board; those two go out as datagrams, which may be lost but never wait
behind board updates. By default it
uses the public OpenDHT network through Jami's bootstrap node and Cloudflare's and
Google's public STUN servers, which get through most home routers. People behind strict
NATs (common on mobile and corporate networks) reach each other through PeerChalk's free
TURN relay, which is limited per IP address to what whiteboards need;
`deploy/coturn/` sets up such a relay on a small server. Settings → Network changes this (from the next start), and
these environment variables take precedence over it:

| Variable | Meaning |
|---|---|
| `PEERCHALK_NETWORK=off` | Stay offline |
| `PEERCHALK_BOOTSTRAP=host:port,…` | OpenDHT bootstrap nodes |
| `PEERCHALK_DHT_NETWORK=<number>` | OpenDHT network id |
| `PEERCHALK_STUN=host:port,…` | STUN servers |
| `PEERCHALK_TURN=user:password@host:port` | A TURN server (UDP), or `none` |

To try it on one computer, run two installations with separate data folders:

```sh
PEERCHALK_DATA_DIR=/tmp/peerchalk-ana bin/peerchalk &
PEERCHALK_DATA_DIR=/tmp/peerchalk-ben bin/peerchalk &
```

Create a project in one, copy the invite from "People & invite" and join with it in
the other. Within about ten seconds the first one shows "1 asking to join"; approve it
under Requests (comparing fingerprints if you like) and the boards start syncing.

## Data on this device

PeerChalk keeps its data in the platform's app data folder (on Linux
`~/.local/share/PeerChalk`), or in the folder named by `PEERCHALK_DATA_DIR`. Accounts are
local: there is no account server. `session.json` names the signed-in account and each
account has a folder `accounts/<id>/`, with `account.json` and:

- `identity.json`: your user key's Ed25519 seed; `device.json`: this device's Ed25519
  seed and X25519 secret. Both are readable only by you but not encrypted; an OS
  keychain is planned.
- `profile.json`, `contacts.json`: your name and colour, and the people you know with how
  you verified them.
- `workspace.json`: projects and boards with their signed access changes and discovery
  secrets (readable only by you).
- `boards/<id>.yrs`: each board's Yrs document.

Settings → Account saves encrypted `.peerchalk` backups (Argon2id and
XChaCha20-Poly1305) of everything, of your identity and known people, or of known people
alone. Device keys are never included; restoring creates a new one. Importing a backup of
an account already on the device merges it. See
[the public release requirements](docs/PUBLIC_RELEASE.md) for what is still needed.

## Layout

- `src/board/`: the board model in plain C++: strokes, their binary encoding and
  triangle meshes, input filtering for pressure-sensitive drawing, the pan/zoom viewport
  and background grid, and the Yrs-backed board with eraser and per-user undo.
- `src/library/`: identities, the signed access log and its rules, the workspace of
  projects and boards on disk, and the invite and contact codes people exchange.
- `src/sync/`: the p2pdc network service and PeerChalk's sync protocol.
- `src/ui/`: the Qt Quick items (canvas, minimap, board previews), the `Library` QML
  singleton, and the screens (home, identity, board), built as the `PeerChalk` QML module.
  Colours and sizes follow the "Paper" design in `Theme.qml`.
- `src/app/`: the application entry point.
- `tests/`: GoogleTest unit tests for the model and the canvas.

## Board controls

| Action | Input |
|---|---|
| Pen, highlighter, eraser, pan | Tool rail, or `P`, `M`, `E`, `H` |
| Pan with any tool | Right- or middle-button drag, or scroll wheel |
| Zoom | `Ctrl` + scroll, `Ctrl` + `=`/`-`, or the zoom buttons |
| Fit all strokes / reset view | `Shift` + `1` / `Ctrl` + `0` |
| Undo | `Ctrl` + `Z` |

The pin button at the top of the right-hand panel switches auto-hide on and off. When
auto-hide is on (or the window is narrower than 720 px), the panel slides over the board
when you hover the right edge or tap the tab there, and hides when the pointer leaves it.
The minimap and zoom controls sit at the bottom of the panel while it is shown, and at
the bottom right of the board otherwise. The auto-hide choice is not yet remembered
between runs.

Pen strokes follow the pressure of tablets and styluses (Qt delivers pen input as mouse
events that keep the pen's pressure); mice and touch draw at full width. The highlighter
has constant width and is drawn beneath the ink at 47% opacity without darkening where
it overlaps itself. The pressure handling, stroke filtering and highlighter look are
modelled on Xournal++'s behaviour, reimplemented here rather than copied.

The eraser removes whole strokes. Strokes are drawn with custom scene-graph geometry,
which Qt's software renderer (`QT_QUICK_BACKEND=software`) does not display.
