# Embedding PeerChalk in another app

PeerChalk's browser build can run inside another web app's page and show one board, with the
host keeping the board's files. The VPvsVNP dashboard does this for boards kept in a git
repository; the message format and the rules for those boards are in
[board-files.md](board-files.md), "Hosted mode". This page is what a host needs to know.

## What the host does

1. **Serve the browser build from its own origin**: `deploy/web/prepare-site.sh <base path>
   <folder>` makes the site; the host serves that folder, for example under `/peerchalk/`.
   PeerChalk checks that every message comes from its own origin, so the frame and the host's
   page must share it.
2. **Show it in a frame** with `?hosted=1`:

   ```html
   <iframe src="/peerchalk/index.html?hosted=1" allow="clipboard-read; clipboard-write"></iframe>
   ```

   Hosted, PeerChalk shows no accounts, home screen or settings: only the board it is given.
   It stores nothing in the browser (no IndexedDB), stays out of the user's own devices'
   space, and uses the default discovery servers.
3. **Talk to it with `postMessage`**, on the frame's `contentWindow`, with the page's own
   origin as the target. It sends `ready`; the host answers `open` with the board, the user's
   identity and, optionally, `host: {backLabel}`. From then on PeerChalk sends `save` after
   edits and the host answers `pushed` or `pushFailed` when it commits; the host sends
   `update` when the board's files change elsewhere. The full table is in board-files.md.
4. **Handle leaving.** With a `backLabel`, the board shows a Back button labelled
   "Back to <backLabel>"; pressing it sends `back` after a last `save`, and the host navigates
   wherever it wants. Before the page goes, the host sends `close` and waits for `closed`
   where it can; a page that unloads cannot wait, so the host should also be able to commit
   what the last `save` gave it on its own.

## What the user sees

- The board, its tools and the people on it, as in the app.
- The board's name with a **connection button** beside it. Its dot is green while this device
  reaches the others (or nobody else has the board open), blue while it is still finding them,
  and red when something is wrong. Clicking it opens the connection pane: what is going on in
  plain words, what to try when it does not work, who is on the board and how each person is
  connected, the servers PeerChalk finds them through, and the connection log, which can be
  copied or downloaded to send to whoever helps.

## Inside PeerChalk

- `src/hosted/`: the session with the host (`HostedSession`), the messages and the account it
  prepares from `open`. `src/app/web_hosted.js` is the page's half of the bridge.
- `HostLink` (`src/ui/host_link.hpp`), a QML singleton: `embedded`, `backLabel` and `back()`.
  Pages use it instead of their own navigation when embedded.
- `Network.boardConnection(boardId)` (`src/sync/network_board_status.cpp`) and
  `src/sync/board_diagnosis.hpp`: what the connection pane shows. `ConnectionAdvice.qml`
  holds its words.
