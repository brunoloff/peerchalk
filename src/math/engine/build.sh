#!/usr/bin/env bash
# Builds the math engine one job at a time and at low priority, into
# p2p-discovery-connect/.deps/peerchalk-math-target. Without arguments it builds the static
# library for this computer. With "web" it builds the module for the browser's math worker;
# Emscripten must be active. That module is separate from the app because Rust's WebAssembly
# panics use wasm exceptions and the Qt kit uses JavaScript ones.
set -euo pipefail

here=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
p2pdc=${PEERCHALK_P2PDC_SOURCE_DIR:-$here/../../../../p2p-discovery-connect}
export CARGO_HOME=${CARGO_HOME:-$p2pdc/.deps/cargo-reference}
export CARGO_TARGET_DIR=${CARGO_TARGET_DIR:-$p2pdc/.deps/peerchalk-math-target}
export CARGO_BUILD_JOBS=1

arguments=(build --manifest-path "$here/Cargo.toml" --release --locked --jobs 1)
case ${1:-} in
    '') arguments+=(--lib) ;;
    web)
        arguments+=(--bin peerchalk_math_worker --target wasm32-unknown-emscripten)
        link=""
        for option in MODULARIZE=1 EXPORT_NAME=PeerChalkMath ENVIRONMENT=worker STACK_SIZE=5MB \
            ALLOW_MEMORY_GROWTH=1 EXPORTED_RUNTIME_METHODS=HEAPU8; do
            link="$link -C link-arg=-s$option"
        done
        export CARGO_TARGET_WASM32_UNKNOWN_EMSCRIPTEN_RUSTFLAGS=${link# }
        ;;
    *) printf 'usage: build.sh [web]\n' >&2; exit 2 ;;
esac
nice -n 15 ionice -c 3 cargo "${arguments[@]}"
