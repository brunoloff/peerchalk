#!/usr/bin/env bash
# Builds the math engine at low priority into CARGO_TARGET_DIR (default: build/math-engine).
# Without arguments it builds the static library for this computer. With "web" it builds the
# module for the browser's math worker; Emscripten must be active. That module is separate from
# the app because Rust's WebAssembly panics use wasm exceptions and the Qt kit uses JavaScript
# ones. The deps preset normally runs this.
set -euo pipefail

here=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
export CARGO_TARGET_DIR=${CARGO_TARGET_DIR:-$here/../../../build/math-engine}
export CARGO_BUILD_JOBS=${CMAKE_BUILD_PARALLEL_LEVEL:-1}

arguments=(build --manifest-path "$here/Cargo.toml" --release --locked)
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
