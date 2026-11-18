#!/usr/bin/env bash
# Builds PeerChalk for browsers into build-web, one job at a time and at low priority.
# Uses the Qt WebAssembly kit and the Emscripten, libsodium and Yrs builds prepared for
# it in p2p-discovery-connect/.deps (see examples/shared-text/README.md there).
set -euo pipefail

root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
p2pdc=${PEERCHALK_P2PDC_SOURCE_DIR:-$root/../p2p-discovery-connect}
qt=${PEERCHALK_QT_ROOT:-$HOME/Qt/6.11.2}
deps=$p2pdc/.deps
yrs_include=$deps/y-crdt-qt/tests-ffi/include
yrs_library=$deps/y-crdt-qt-target/wasm32-unknown-emscripten/debug/libyrs.a
sodium=$deps/sodium-qt-wasm

for needed in "$qt/wasm_singlethread/bin/qt-cmake" "$qt/gcc_64/bin" \
    "$deps/emsdk-qt/emsdk_env.sh" "$yrs_include/libyrs.h" "$yrs_library" \
    "$sodium/include/sodium.h" "$sodium/lib/libsodium.a"; do
    if [ ! -e "$needed" ]; then
        printf 'build.sh: missing %s\n' "$needed" >&2
        exit 1
    fi
done

export EMSDK_QUIET=1
# shellcheck disable=SC1091
source "$deps/emsdk-qt/emsdk_env.sh"
export EM_CACHE=$deps/emscripten-qt-cache EMCC_CORES=1

"$root/src/math/engine/build.sh" web
sh "$qt/wasm_singlethread/bin/qt-cmake" -S "$root" -B "$root/build-web" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DQT_HOST_PATH="$qt/gcc_64" \
    -DPEERCHALK_P2PDC_SOURCE_DIR="$p2pdc" \
    -DPEERCHALK_YRS_INCLUDE_DIR="$yrs_include" \
    -DPEERCHALK_YRS_LIBRARY="$yrs_library" \
    -DP2PDC_ENABLE_NATIVE_TRANSPORT=OFF -DP2PDC_ENABLE_OPENDHT=OFF \
    -DP2PDC_ENABLE_SODIUM=ON -DP2PDC_BUILD_SHARED_TEXT=OFF \
    -DSODIUM_INCLUDE_DIR="$sodium/include" \
    -DSODIUM_LIBRARY="$sodium/lib/libsodium.a"
nice -n 15 ionice -c 3 cmake --build "$root/build-web" --parallel 1 --target peerchalk
rm -rf "$root/build-web/site"
"$root/deploy/web/prepare-site.sh" / "$root/build-web/site"
