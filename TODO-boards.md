# Repository boards: what is left

The plan is [docs/board-files.md](docs/board-files.md); its milestones are numbered the same way
here. Work lives on three local branches, none pushed yet:

| Repository | Branch | Where |
|---|---|---|
| p2pdc | `seal-to-signing-keys` | `p2p-discovery-connect/.claude/worktrees/seal-signing-keys` |
| PeerChalk | `claude/repo-boards-f83b55` | `PeerChalk/.claude/worktrees/repo-boards-f83b55` |
| VPvsVNP | `dashboard-boards` | `~/Crapbox/VPvsVNP/etc/worktrees/dashboard-boards` |

## Done

- [x] 1. p2pdc: sealing to user keys
- [x] 2. Repository format in PeerChalk (`src/repository/`)
- [x] 3. `peerchalk-board`
- [x] 4. Dashboard git layer (`dashboard/boards/`)
- [x] 5. Hosted mode and the scribe in PeerChalk
- [x] 6. Dashboard Boards tab, and the end-to-end test (two clones, two dashboards, live)
- [x] Dashboard binaries out of the repository: pinned releases (`dashboard/release.txt`)
- [x] Embedding: Back leads to the host ([docs/embedding.md](docs/embedding.md)); the board's
      connection button and pane, with advice and the log; the dashboard's board page at full
      width; the board list filtered to your boards, with pins

- [x] Embedded: no home screen under the board, a plain loading card; the connection button
      and pane on phones
- [x] 7. People in the dashboard: removal (snapshot, new secret epoch) in the board page's
      drawer; warnings and deleted access entries shown there
- [x] 8. Compaction by the dashboard: past 1 MiB of uncovered edits as the page closes, and a
      button (`peerchalk-board compact --above`)
- [x] 9. Releasing the pieces: `release.yml` builds `peerchalk-web-<v>.zip` and
      `peerchalk-board-<v>-<system>-<arch>.zip` (the program with its libraries, checked by
      running it unpacked); the dashboard unpacks them, and `dashboard/pin-peerchalk.sh`
      pins a release. Tried on Linux only, through a local stand-in release.

## Publishing what is done

- [x] Push p2pdc `seal-to-signing-keys` (PeerChalk's `cmake/P2pdcSource.cmake` pins its commit, so
      it goes first).
- [x] Push PeerChalk `claude/repo-boards-f83b55` (to `main`, as 0.4.0).
- [ ] Release PeerChalk with these pieces (a version tag runs `release.yml`); check that the
      macOS and Windows `peerchalk-board` packages pass their run check there, which has
      never run.
- [ ] `dashboard/pin-peerchalk.sh <version>` in VPvsVNP, commit `dashboard/peerchalk.json`;
      rebuild the dashboard binaries (`dashboard/build.sh`; the pinned 20261126 ones are older
      than this work) and upload them as their release, *before* pushing `dashboard-boards`
      (`build.sh` prints the command).
- [ ] Merge and push VPvsVNP `dashboard-boards`. After pulling, everyone runs
      `dashboard/run install` again (old launchers point at the removed `dashboard/bin/`).
- [ ] Try `dashboard\run.cmd` on a Windows machine: it was written but not run.

## Milestones left

- [ ] 10. The app: "Open board folder…".
- [ ] 11. Single file `.peerchalk` and `peerchalk-pdf`.
- [ ] 12. Release 0.4.

## Smaller follow-ups

- [ ] Blobs and PDFs in saves (hosted saves carry strokes only).
- [ ] Board names and co-author trailers in the scribe's commit messages.
- [ ] After a removal, frames already open keep the old network secret until they reopen:
      `update` could carry the new one.
- [ ] A hermetic end-to-end test that does not use the public discovery servers.
- [ ] The dashboard's tray panics on exit when there is no session bus (older than this work;
      it breaks Restart in that case).

## Decided

- 2026-11-26: keep signatures for repository boards rather than trusting git push access, so
  authorship holds under the scribe and union merges, boards can include people outside the
  repository, and names in a live session are verified.
