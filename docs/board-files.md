# Repository boards — plan for PeerChalk 0.4

PeerChalk 0.4 lets boards live in a git repository, so a group that already shares a repository
also shares its boards there. The first user is the VPvsVNP research dashboard: a local Go web
server inside the group's repository, which lists the repository's boards and shows them in an
embedded PeerChalk. Two people who open the same board at once find each other and edit it live,
exactly as in the app; the edits reach the repository on their own.

0.4 also keeps the earlier plan's single-file form, `.peerchalk`, now as the export or packed form
of the same content, and the standalone `peerchalk-pdf` converter.

## Summary

- Boards live on a branch of their own, `peerchalk-boards`, written with git plumbing. The user's
  checkout, current branch and index are never touched.
- Every file on that branch is either immutable (named by its SHA-256) or owned by one device
  (only that device writes it). A merge of two versions of the branch is therefore a union, and
  never conflicts.
- While people edit together, one of them, the **scribe**, commits and pushes for everyone.
- Who may do what is PeerChalk's existing signed access log (admin, owners, members), stored as
  immutable files. PeerChalk loads only content signed by members' devices and connects only to
  members' devices.
- The dashboard runs PeerChalk's browser build in a frame, in a **hosted mode** without accounts,
  and talks to it with `postMessage`. Git is the dashboard's job; live sync is PeerChalk's.
- A small command-line tool, **`peerchalk-board`**, does for the dashboard what needs PeerChalk's
  own code: reading Yrs documents, checking signatures, sealing secrets.

## The branch

- Name: `peerchalk-boards`, an orphan branch (no common history with the repository's other
  branches). It holds only the layout below.
- The dashboard reads it with `git ls-tree` and `git cat-file --batch`, and writes it with
  `hash-object -w`, `mktree`, `commit-tree` and `update-ref refs/heads/peerchalk-boards`. It
  never checks the branch out, and builds trees without an index (or with a temporary one through
  `GIT_INDEX_FILE` under `.git/peerchalk/`).
- **Pushing:** fetch `origin peerchalk-boards`; if the remote moved, union merge (below) and
  commit the merge with both parents; push; if the push is rejected because someone pushed
  meanwhile, start again, up to five times with a short random wait. Pushing never forces.
- **Union merge**, path by path, against the merge base:
  - a path one side left as it was in the base takes the other side's version, deletion included;
  - an immutable file is the same on both sides whenever it exists (its name is its hash), so
    it exists unless one side deleted it;
  - an owned file changed on both sides takes the version of its owner's side: our own device's
    file from our side, any other device's from theirs. If our own file was changed elsewhere
    (the same device key in two clones, which setup prevents), ours wins and the dashboard
    warns;
  - a path that is neither immutable nor owned is malformed: kept from our side, with a warning.
- **Branch protection:** we recommend protecting `peerchalk-boards` on GitHub (no force pushes, no
  deletion). The dashboard's README says how. Nothing depends on it: see "Deleted entries".

## Files

```
people/<user-key>.json             a person: name, user key, device certificates; owned by that user
boards/<id>/board.json             board id, creator, format version; immutable (written once)
boards/<id>/access/<sha256>.op     one signed access entry (AccessOp payload and signature)
boards/<id>/snapshots/<sha256>.yrs a signed compacted state
boards/<id>/changes/<device>.yrs   signed, owned by that device: all it knows beyond the snapshots
boards/<id>/blobs/<sha256>         PDFs and other files the board uses
boards/<id>/secrets/<epoch>/<signer>/<user>  the board's network secret, sealed to that member
```

Keys in paths are lowercase hex: user keys and device keys are 32-byte Ed25519 public keys.
Hashes are SHA-256: of the whole file for snapshots and blobs, and of the signed payload for
access entries (so an entry has one name however it reached a device).

- **`people/<user-key>.json`:** `{"format": "peerchalk-person", "version": 1, "name": ...,
  "userKey": ..., "devices": [{"payload": ..., "signature": ...}], "signature": ...}`. The
  devices are PeerChalk's existing device certificates (`signatures.hpp`). The whole file is
  signed by the user key, and only that person's dashboard writes it. It gives names to keys in
  the dashboard and lets owners pick people to add; access never depends on it.
- **`board.json`:** `{"format": "peerchalk-repository-board", "version": 1, "board": <uuid>,
  "creator": <user key>}`. The creator is the access log's trust anchor (as an invite's creator
  is today). The board's **name** lives in the Yrs document (`meta.name`), so renaming is an edit
  like any other; the access log's `Create` carries the first name, for boards that have no
  content yet.
- **Signed content files** (`snapshots/`, `changes/`) share one envelope:

  ```
  "PCRB" 0x01                      magic and envelope version
  u32 header length, header        compact JSON, below
  u32 body length, body            a Yrs v1 update
  64 bytes                         Ed25519 signature by the device key over everything before
  ```

  The header is `{"board": <uuid>, "kind": "snapshot"|"changes", "device": <device key>,
  "certificate": {payload, signature}, "covers": [<snapshot hashes>], "clients": [<Yrs client
  ids>], "stateVector": <base64>}`. The state vector is everything the device knew when it
  wrote the file: a change file's body builds on snapshots, so the body alone can show less.
  The device uses it to decide when its own file is covered; for snapshots, readers measure
  the body itself, so a forged state vector can make nobody delete anything. The certificate ties the device to a user, so a file can be
  checked on its own. `covers` lists the snapshots the body is relative to (for a change file) or
  that it replaces (for a snapshot). `clients` lists the Yrs client ids this device has written
  with, for "each device answers for its own client ids" below.
- **Blobs** are checked against their names; they are not signed, since only signed content can
  refer to them.
- **Secrets:** a board's network secret (its p2pdc discovery capability) for one epoch, sealed to
  one member's user key (see "Sealing to user keys") and signed by whoever sealed it, the
  creator or an owner, who hand on the secret they hold. Each removal starts a new epoch. The signer is in
  the path, so two owners who start the same epoch at once never write the same file;
  everyone uses the secret from the lowest signer key that sealed one to them.

### Loading

Loading a board applies every accepted snapshot and change file to an empty Yrs document. Yrs
merges updates in any order, and applying one twice changes nothing, so the result does not depend
on how the files were merged.

### Compaction

A device with the board loaded writes a snapshot of its whole state (`covers` lists the snapshots
it loaded), deletes those snapshots, and deletes its own change file in the same commit. Other
devices delete their own change file once the accepted snapshots cover its state vector.
Two concurrent compactions leave two snapshots after the union merge; both are valid, nothing is
lost, and the next compaction folds them together. The dashboard compacts when a board's change
files total more than 1 MiB, or when asked.

## Roles and what PeerChalk enforces

Each board has an admin (its creator), who appoints owners; owners add members. This is the
existing signed access log (`access_log.hpp`, `access_rules.cpp`), each entry stored as an
immutable `access/<sha256>.op`. The rule that members edit only while an owner is connected is
dropped for repository boards: members may edit at any time.

- **Content:** PeerChalk applies a snapshot or change file only if its envelope signature is
  valid, its certificate names its device, and the certificate's user is a member or above.
  Everything else is skipped with a warning naming the file (shown in the dashboard).
- **Live:** in a repository board's session PeerChalk syncs only with devices whose user is a
  member or above (the existing `sharesBoard` check, without the owner rule).
- **Removing someone:** the owner's device first writes a snapshot of everything it knows, then
  the removal entry (`SetRole` with role None), which gains a field naming that snapshot. Once the
  removal is in the log, every file signed by that person's devices is ignored; the snapshot
  keeps what they wrote before. The owner then writes a new epoch of the network secret, sealed
  to the remaining members, so the removed person can no longer find the session.
- **Deleted entries:** the access log is the union of every access file ever committed to the
  branch, read from its history (`git log --diff-filter=A` on `boards/<id>/access/`), plus every
  entry this clone has seen (kept in `.git/peerchalk/seen/`). Deleting an entry, even with a
  force push, changes nothing; the dashboard warns that it happened. The same holds for
  `board.json`: its first committed version is the one that counts.

## The scribe

In a live session, one device commits for everyone.

- **Who:** the connected device with the lowest device key (bytewise) whose user is a member or
  above and whose dashboard can push. Every device works this out from what it knows, so there is
  no vote: a device that sees itself as the lowest eligible key is the scribe. Two scribes for a
  moment (a split, a late hello) only make two commits, which union merge combines.
- **What:** about every 30 seconds while there are edits, and once the session is idle for 5
  seconds, the scribe has its dashboard commit its own change file, which holds everything the
  session knows beyond the snapshots, and push. Then it tells the session what it pushed.
- **Others** do not commit while a scribe is present. Each device still saves its own change file
  locally after every edit, outside the repository's files (`.git/peerchalk/pending/`), so a
  crash loses nothing.
- **Each device answers for its own Yrs client ids.** When a device leaves or closes, it checks
  whether the latest "pushed" announcement covers its client ids. If not, it asks the scribe to
  push now and waits up to 10 seconds; failing that, it commits and pushes its own change file
  itself if it can, and otherwise keeps it pending for the next time.
- **Alone,** a device is its own scribe. A member whose dashboard cannot push edits fine while a
  scribe is present; alone, their edits wait in the pending file until a scribe appears (live sync
  then hands them over) or until they can push.
- **Commits** carry the scribe's own git identity, with the session's authors since the last
  commit as `Co-authored-by:` trailers (name and, when their `people/` file gives one, email).
  Message: `Board <name>: edits by <names>`.

### Over PeerChalk's sync messages

Protocol 5 adds two messages, which protocol 4 peers ignore as `UnknownMessage`:

```
RepoStatus { board, canPush: bool, pushed: optional<Pushed> }   on connecting and on change
RepoPushed { board, commit: 20 bytes, stateVector: bytes }      after each push, and relayed
```

`RepoStatus` says whether the sender can be the scribe. `RepoPushed` says that the branch at
`commit` holds the board up to `stateVector`; a device whose client ids are covered by it has
nothing left to answer for. Both go only to peers that share the board (`sharesBoard`); sessions
are a full mesh, so neither is relayed. A `RepoPushed` only ever makes a device skip a push it
would otherwise ask for: believing a false one at worst leaves its edits pending locally until
the next time.

The election and the timers live in `sync/repository_scribe.cpp` and `hosted/hosted_session.cpp`;
`Network` only routes the two messages and tells the scribe when peers come and go.

## Hosted mode: PeerChalk in the dashboard

How another app embeds PeerChalk in general is in [embedding.md](embedding.md).

The dashboard serves PeerChalk's browser build and shows it in a frame (`<iframe
src="/peerchalk/<version>/index.html?hosted=1">`, same origin as the dashboard). With `hosted=1`
PeerChalk:

- shows no accounts or welcome screens, only the board it is given, and no other boards;
- keeps nothing in IndexedDB (the dashboard keeps everything), and claims no instance slot;
- uses the user identity and device key the dashboard gives it, and the default discovery servers;
- stays out of the user's personal space, so their own devices never swap workspaces with it;
- talks to the dashboard only through `window.parent.postMessage`, checking that messages come
  from its own origin.

### Messages

Every message is `{type, ...}`. Binary values are `Uint8Array`s (structured clone, no base64).
Keys and hashes are lowercase hex strings.

Dashboard → PeerChalk:

| type | fields | when |
|---|---|---|
| `open` | `protocol: 1`, `board: {id, files: {path: Uint8Array}}` (the branch's files with this clone's pending ones on top), `access: [Uint8Array]` (every entry, history and seen ones included), `secret: Uint8Array`, `identity: {transferCode, deviceSeed, recipientSecret, deviceName, name, colorIndex}`, `canPush`, `clients: [string]` (Yrs client ids this device used before), `host?: {backLabel}` (where the board's Back button leads, such as "Boards"; without it there is no Back button) | once, after `ready` |
| `update` | `files: {path: Uint8Array \| null}`, `access: [Uint8Array]`, `canPush?` | after a pull brings new files |
| `pushed` | `revision`, `commit` | the commit asked for has been pushed |
| `pushFailed` | `revision`, `error` | pushing failed (the files stay pending) |
| `close` | — | the user leaves the page: flush, then answer `closed` |

PeerChalk → dashboard:

| type | fields | when |
|---|---|---|
| `ready` | `protocol: 1`, `version` | the module has started |
| `save` | `board`, `revision`, `files: {path: Uint8Array \| null}` (its own change file, the access entries it knows, later blobs and compactions), `clients: [string]`, `push: bool`, `authors: [{name}]` | half a second after edits stop, and when scribing (`push: true`) |
| `warning` | `text` | a file was skipped, a message was refused |
| `back` | — | the user pressed Back, after a last `save`: the host leaves the board (and sends `close` as the page goes) |
| `closed` | — | answer to `close`, after the last `save` |

The identity enters the frame whole, as PeerChalk's transfer code plus the device's secrets: the
frame is the dashboard's own origin, so keeping the user seed outside it would protect nothing,
and the app signs access entries itself. A page that closes takes its frame with it, so the
dashboard's page also sends a beacon (`POST /board/<id>/closed`) on `pagehide`, and the dashboard
then commits and pushes that clone's pending files itself.

## The dashboard (VPvsVNP)

- **Boards tab:** the repository's boards (name, last edit, who), "New board", and a page per
  board with the frame. The list comes from `peerchalk-board list`, cached per branch commit.
- **Sync setting:** "Sync boards automatically" (on by default): pull the branch every minute
  while a board is open and every 10 minutes otherwise, and push when PeerChalk asks. The Boards
  tab shows the last push (time, commit, result). Off, the dashboard only commits locally; the
  Push button then also pushes `peerchalk-boards`.
- **Credentials:** pushing and fetching run the user's own `git` with `GIT_TERMINAL_PROMPT=0`,
  so they use whatever credentials git already has, and fail with a message instead of asking.
- **Identity:**
  - the user identity (user seed, `identity.json`, as in PeerChalk) lives in the user's
    configuration folder, `<UserConfigDir>/peerchalk-board/` (or `PEERCHALK_BOARD_USER_DIR`),
    shared by all their clones. It is created on first use, or imported from PeerChalk with its
    existing transfer code (`peerchalk-identity:1:…`, "Link a device" in the app);
  - the device key lives in `.git/peerchalk/device/device.json`, one per clone, so two clones
    are two devices (and two dashboards on one computer can edit together). The dashboard writes
    the device's certificate into the person's `people/` file when the Boards tab opens.
- **Serving PeerChalk:** `dashboard/peerchalk.json` (committed) pins a PeerChalk version and the
  SHA-256 of each file the dashboard needs from that GitHub release: `peerchalk-web-<v>.zip` and
  `peerchalk-board-<os>-<arch>`. The dashboard downloads them on first use into
  `<UserCacheDir>/peerchalk-board/<v>/`, checks the hashes, and serves the web build at
  `/peerchalk/<v>/`. `PEERCHALK_DIST=<folder>` uses a local build instead, for development and
  tests. Nothing big is committed to the group's repository.

## `peerchalk-board`

A command-line program built from PeerChalk's library code (Qt Core, Yrs, p2pdc's crypto; no GUI).
Boards come as JSON on standard input, `{"files": {path under boards/<id>/: base64}, "access":
[base64 entries from the history]}`, so it never touches git; every answer is one JSON object.

```
peerchalk-board --user-dir U --device-dir D <command>
  identity                            this device's keys and certificate
  identity-import <transfer code>     become the user of PeerChalk's "Link a device" code
  device-secrets [--name N]           what a hosted PeerChalk needs (private)
  person --name N [--email E]         this person's people/<user>.json
  create --name N [--display-name D]  a new board's files
  list     (stdin {"boards": {id: board}})  name, role, people, strokes, warnings of each
  open     (stdin board)              one board, with its network secret if sealed to you
  set-role (stdin board) --user K --role member|owner [--name N]
  remove   (stdin board) --user K [--clients C]   snapshot, removal, new secret epoch
  compact  (stdin board) [--above BYTES] [--clients C]
later: export, import
```

`compact` folds the board into one snapshot when the change files the snapshots do not yet
cover hold more than `BYTES` (default 0), and answers `compacted: false` otherwise; either way
it deletes this device's own change file once snapshots cover it. `--clients` lists the Yrs
client ids this device used, comma-separated.

Exit codes: 0 done, 1 bad command line or input, 2 unreadable files, 3 refused (signatures,
rights).

### Sealing to user keys

Secrets are sealed to a member's **user** key, so any of their devices can open them and a new
device needs nothing re-sealed. p2pdc gains `seal_to_signing_key` and `Identity::open_sealed`,
which convert Ed25519 keys to X25519 (libsodium's `crypto_sign_ed25519_pk_to_curve25519` and
`…_sk_to_curve25519`) and use its sealed boxes, on a p2pdc branch of its own.

## Opening a repository board in the app

The standalone app does not list repository boards, but can open one: "Open board folder…" on a
`boards/<id>/` folder (from a checkout or worktree of `peerchalk-boards`). It joins the live
session with the user's own identity, reads the folder's files, and writes only its own change
file there; committing is left to the user. This comes after the dashboard milestones.

## The single-file form: `.peerchalk`

The earlier plan's zip archive stays, as the export or packed form of the same content:
`manifest.json`, `board.yrs` (the whole Yrs document), `blobs/<sha256>`, and optional
`preview.png`, written in a fixed order with fixed timestamps so the same board always gives the
same bytes. It carries no access, secrets or keys: whoever opens one becomes the admin of their
copy, or merges it into the same board if they already have it. The app saves and opens it;
`peerchalk-board export` and `import` convert between it and a repository board. The git merge
driver of the earlier plan is no longer needed.

The schema rules of the earlier plan also stay: `src/board/board_schema.hpp` names the document's
parts and `boardSchemaVersion`; `boardBlobs(const Board &)` lists every blob a board uses;
readers keep entries they do not understand; fixtures for every schema version are kept forever.

## `peerchalk-pdf`

Unchanged from the earlier plan: `peerchalk-pdf [--column ID] [--pages A-B] <board.peerchalk |
board folder | -> [-o out.pdf]`, with the app's own renderer moved into a `peerchalk_export`
library; a desktop program (offscreen Qt platform, bundled fonts) and a WebAssembly module for web
pages (`loadConverter()`), not for Node.js or workers. It also reads a repository board folder.

## Tests

PeerChalk (Qt Test, under `tests/`):

- **Envelope and loading:** a valid file is applied; a forged signature, an unsigned file, a
  certificate for another device, a certificate whose user is not a member, a header naming
  another board, and a truncated file are each skipped with the right warning. Loading files in
  every order gives the same document.
- **Roles:** a removal entry deleted from the branch still counts (union with history and the seen
  cache); after a removal, the removed person's change files are ignored and the snapshot keeps
  their earlier work; a member removed mid-session is disconnected and their later files are
  ignored; a removal that names no snapshot is refused.
- **Compaction:** two concurrent compactions merged by union keep every edit; a device empties
  its change file only once a snapshot covers it.
- **Scribe:** the election as a pure function (lowest eligible key; non-members and devices
  that cannot push are skipped; ties and changes as peers come and go); `RepoStatus` and
  `RepoPushed` encode, decode, and are ignored by protocol 4.
- **Hosted bridge:** `open`, `update` and `save` round trips in a headless Chromium run of the
  browser build.
- **`peerchalk-board`:** each command's output and exit codes; sealing and opening a secret.

Dashboard (Go tests, in `dashboard/`), against temporary bare repositories:

- writing the branch never changes the checkout, the current branch or the index;
- union merge: owned files of different devices, both sides adding immutable files, one side
  deleting covered snapshots, a malformed path; the result for every pair of histories is the
  union;
- rejected pushes: someone pushes between fetch and push, and the retry succeeds; five rejections
  give up and keep the commit locally;
- a deleted access entry and a force-pushed branch are noticed and reported;
- the pinned download refuses a file whose hash does not match.

End to end (`dashboard/tests/e2e`, run by hand and in the dashboard's CI if it gets one): a bare
repository, two clones, two dashboards on two ports, a local PeerJS server and MQTT broker for
discovery, and headless Chromium driving both frames. Both draw on one board at once and see each
other's strokes; both close; the branch then holds every stroke, from a scribe's commits, with no
conflict, and a fresh clone loads the same board.

## Milestones

1. **p2pdc: sealing to user keys** (branch of its own), with tests.
2. **Repository format in PeerChalk:** `src/repository/` — the envelope, `board.json`, `people`,
   loading with enforcement, removal with its snapshot, compaction; the board name in the
   document. Unit tests above.
3. **`peerchalk-board`:** identity, person, create, list, check, open-secret, sign.
4. **Dashboard git layer:** reading and writing the branch, union merge, push with retry, history
   union of access entries, pending files. Go tests.
5. **Hosted mode and the scribe in PeerChalk:** the `postMessage` bridge, a hosted library with
   one board, the repository-board sync rule, `RepoStatus` and `RepoPushed`, the election.
6. **Dashboard boards tab:** list, new board, the frame, serving a pinned or local build, the
   sync setting and last push. The end-to-end test. *This is where two clones edit live and the
   scribe pushes.*
7. **People in the dashboard:** adding members and owners, removal with snapshot and a new
   secret epoch, warnings shown.
8. **Compaction** done by the dashboard.
9. **Releasing the pieces:** `peerchalk-web-<v>.zip` and `peerchalk-board-<os>-<arch>` in the
   release workflow; `dashboard/peerchalk.json` pinned to it.
10. **The app:** "Open board folder…".
11. **Single file and `peerchalk-pdf`:** schema, `.peerchalk`, saving and opening in the app,
    the converter.
12. **Release 0.4.**

## Risks

- **History growth:** the scribe rewrites a change file every 30 seconds while people edit, and
  git keeps every version. Git's delta compression helps, since each version mostly extends the
  last; compaction keeps change files small. If it still grows too fast, the scribe commits less
  often and squashes its own unpushed commits before pushing.
- **Edits outside the rules:** a member's change file may carry edits by others, merged in live,
  which is how the scribe works. An edit made by someone just removed, merged by a member who had
  not yet heard of the removal, can therefore survive. Live sync refuses non-members as soon as
  the removal arrives, so the window is short.
- **Trust in the dashboard:** the frame trusts its own origin, so anything that can run script on
  the dashboard's address can act as the user on boards. The dashboard listens only on
  `127.0.0.1` and its API needs a token; the PeerChalk pages need neither, since they hold
  nothing until `open`.
- **Two scribes:** in a split session each side has a scribe; both push, and union merge combines
  them. Only the number of commits suffers.
- **Browser build size:** about 32 MB, downloaded once per pinned version.
- **Yrs encoding:** the v1 update encoding is Yjs's documented wire format, which sync already
  depends on; fixtures catch a change.
