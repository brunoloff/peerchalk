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

Lint with `clang-format --dry-run --Werror` on the C++ sources and
`cmake --build build-native --target all_qmllint` for the QML.

## Layout

- `src/board/`: the board model in plain C++ (strokes and their triangle meshes).
- `src/ui/`: the Qt Quick canvas item and QML, built as the `PeerChalk` QML module.
- `src/app/`: the application entry point.
- `tests/`: GoogleTest unit tests for the model and the canvas.
