#!/usr/bin/env bash
# Installs the toolchains PeerChalk builds with, at pinned versions:
#
#   tools/setup.sh desktop   Rust and Qt for Linux, in ~/Qt
#   tools/setup.sh web       the same, plus the Rust browser target, Qt for browsers and
#                            Emscripten, in .emsdk
#
# Anything already installed is left alone.
set -euo pipefail

readonly qt_version=6.11.2 aqt_version=3.3.0 emscripten_version=4.0.7 rust_version=1.96.1
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
readonly root qt_root=$HOME/Qt

aqt() {
    if [ ! -x "$root/.tools/aqt/bin/aqt" ]; then
        python3 -m venv "$root/.tools/aqt"
        "$root/.tools/aqt/bin/pip" install --quiet "aqtinstall==$aqt_version"
    fi
    (cd "$root/.tools" && "$root/.tools/aqt/bin/aqt" "$@")
}

# install_qt FOLDER ARGUMENTS...: installs a kit unless ~/Qt already has FOLDER.
install_qt() {
    local folder=$1
    shift
    [ -d "$qt_root/$qt_version/$folder" ] || aqt install-qt "$@" -O "$qt_root"
}

install_emscripten() {
    if [ ! -d "$root/.emsdk/.git" ]; then
        git -c advice.detachedHead=false clone --depth 1 --branch "$emscripten_version" \
            https://github.com/emscripten-core/emsdk.git "$root/.emsdk"
    fi
    "$root/.emsdk/emsdk" install "$emscripten_version"
    "$root/.emsdk/emsdk" activate "$emscripten_version"
}

main() {
    local target=${1:-}
    case $target in
        desktop | web) ;;
        *) printf 'usage: tools/setup.sh desktop|web\n' >&2; exit 2 ;;
    esac
    mkdir -p "$root/.tools"
    rustup toolchain install "$rust_version" --profile minimal
    install_qt gcc_64 linux desktop "$qt_version" linux_gcc_64
    if [ "$target" = web ]; then
        rustup target add --toolchain "$rust_version" wasm32-unknown-emscripten
        install_qt wasm_singlethread all_os wasm "$qt_version" wasm_singlethread
        install_emscripten
    fi
}

main "$@"
