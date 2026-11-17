# PeerChalk

A peer-to-peer collaborative whiteboard, built on
[p2p-discovery-connect](../p2p-discovery-connect) for peer discovery and
authenticated WebRTC data channels.

Status: early development.

PeerChalk is licensed under the MIT License; see [LICENSE](LICENSE).
Third-party dependencies retain their respective licenses.

## Build

PeerChalk uses C++20, CMake, Qt 6.5 or newer (Qt Quick and Quick Controls 2)
and GoogleTest. It builds
[p2p-discovery-connect](../p2p-discovery-connect) from source, expected next to
this repository (override with `-DPEERCHALK_P2PDC_SOURCE_DIR=...`), so that
library's dependencies must be prepared first; see its README.

The board is a [Yrs](https://github.com/y-crdt/y-crdt) document (the Rust port of
Yjs, used through its C interface), so peers can merge their edits. By default the
build uses the Yrs header and library from p2p-discovery-connect's Yrs reference build
(`.deps/y-crdt`, see its `integrations/yrs/README.md`); set `PEERCHALK_YRS_INCLUDE_DIR`
and `PEERCHALK_YRS_LIBRARY` to use another build. Browser and Android builds need a
Yrs library built for that target.

Native Linux build and tests, one job at a time:

```sh
cmake -S . -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLibDataChannel_DIR="$PWD/../p2p-discovery-connect/.deps/install/lib/cmake/LibDataChannel"
nice -n 15 ionice -c 3 cmake --build build-native --parallel 1
ctest --test-dir build-native -j 1
```

Run the app with `bin/peerchalk`.

## Data on this device

PeerChalk keeps its data in the platform's app data folder (on Linux
`~/.local/share/PeerChalk`), or in the folder named by `PEERCHALK_DATA_DIR`, which is
handy for running a second identity side by side:

- `identity.json`: your Ed25519 signing seed and X25519 recipient secret. It is readable
  only by you but not encrypted; an OS keychain is planned.
- `profile.json`, `contacts.json`: your name and colour, and the people you know with how
  you verified them.
- `projects.json`: projects, their boards, and each project's discovery secret (also
  readable only by you).
- `boards/<id>.yrs`: each board's Yrs document.

An invite code (`peerchalk-invite:1:…`) carries a project's id, name and secret: anyone
with it can join, so share it privately. A contact code (`peerchalk-contact:1:…`)
carries only your public key and name. Compare fingerprints to verify each other.
Sharing boards between members needs the networking that is still to come.

Lint with `clang-format --dry-run --Werror` on the C++ sources and
`cmake --build build-native --target all_qmllint` for the QML.

## Layout

- `src/board/`: the board model in plain C++: strokes, their binary encoding and
  triangle meshes, input filtering for pressure-sensitive drawing, the pan/zoom viewport
  and background grid, and the Yrs-backed board with eraser and per-user undo.
- `src/library/`: this device's projects, boards and identity on disk, and the invite
  and contact codes people exchange.
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
