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

Native Linux build and tests, one job at a time:

```sh
cmake -S . -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLibDataChannel_DIR="$PWD/../p2p-discovery-connect/.deps/install/lib/cmake/LibDataChannel"
nice -n 15 ionice -c 3 cmake --build build-native --parallel 1
ctest --test-dir build-native -j 1
```

Run the app with `bin/peerchalk`.

Lint with `clang-format --dry-run --Werror` on the C++ sources and
`cmake --build build-native --target all_qmllint` for the QML.

## Layout

- `src/board/`: the board model in plain C++: strokes and their triangle meshes, the
  pan/zoom viewport and background grid, and the board with eraser and undo.
- `src/ui/`: the Qt Quick canvas and minimap items and the QML screens, built as the
  `PeerChalk` QML module. Colours and sizes follow the "Paper" design in `Theme.qml`.
- `src/app/`: the application entry point.
- `tests/`: GoogleTest unit tests for the model and the canvas.

## Board controls

| Action | Input |
|---|---|
| Pen, highlighter, eraser, pan | Tool rail, or `P`, `M`, `E`, `H` |
| Pan with any tool | Middle-button drag or scroll wheel |
| Zoom | `Ctrl` + scroll, `Ctrl` + `=`/`-`, or the zoom buttons |
| Fit all strokes / reset view | `Shift` + `1` / `Ctrl` + `0` |
| Undo | `Ctrl` + `Z` |

The eraser removes whole strokes. Strokes are drawn with custom scene-graph geometry,
which Qt's software renderer (`QT_QUICK_BACKEND=software`) does not display.
