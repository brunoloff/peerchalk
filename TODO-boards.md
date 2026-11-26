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

## Publishing what is done

- [ ] Push p2pdc `seal-to-signing-keys` (PeerChalk's `cmake/P2pdcSource.cmake` pins its commit, so
      it goes first).
- [ ] Push PeerChalk `claude/repo-boards-f83b55`.
- [ ] Upload the dashboard binaries as the release `dashboard-20261126`, *before* pushing
      `dashboard-boards` (the command is in `dashboard/README.md`, Building; `build.sh` prints it).
- [ ] Merge and push VPvsVNP `dashboard-boards`. After pulling, everyone runs
      `dashboard/run install` again (old launchers point at the removed `dashboard/bin/`).
- [ ] Try `dashboard\run.cmd` on a Windows machine: it was written but not run.

## Milestones left

- [ ] 7. People in the dashboard: adding members and owners, removal with a snapshot and a new
      secret epoch, warnings shown.
- [ ] 8. Compaction done by the dashboard.
- [ ] 9. Releasing the pieces: `peerchalk-web-<v>.zip` and `peerchalk-board-<os>-<arch>` in the
      release workflow; `dashboard/peerchalk.json` pinned to it. Until then the Boards tab needs
      `PEERCHALK_DIST` pointing at a local build.
- [ ] 10. The app: "Open board folder…".
- [ ] 11. Single file `.peerchalk` and `peerchalk-pdf`.
- [ ] 12. Release 0.4.

## Smaller follow-ups

- [ ] Blobs and PDFs in saves (hosted saves carry strokes only).
- [ ] Board names and co-author trailers in the scribe's commit messages.
- [ ] The connection pane on phones (the phone layout has no title panel, so no button yet).
- [ ] Embedded, PeerChalk still builds its home page under the board; skip it entirely.
- [ ] A hermetic end-to-end test that does not use the public discovery servers.
- [ ] The dashboard's tray panics on exit when there is no session bus (older than this work;
      it breaks Restart in that case).

## Decided

- 2026-11-26: keep signatures for repository boards rather than trusting git push access, so
  authorship holds under the scribe and union merges, boards can include people outside the
  repository, and names in a live session are verified.
