#!/usr/bin/env bash
# Assembles the browser build into a folder a static web host can serve.
#
#   deploy/web/prepare-site.sh BASE_PATH OUTPUT_FOLDER
#
# BASE_PATH is where the app lives on its host, such as /peerchalk/ for GitHub Pages at
# https://USER.github.io/peerchalk/. GitHub Pages cannot route /board/... to the app, so
# 404.html is a copy of it: <base> makes its files load from BASE_PATH on any address.
set -euo pipefail

root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
base=${1:?usage: prepare-site.sh BASE_PATH OUTPUT_FOLDER}
out=${2:?usage: prepare-site.sh BASE_PATH OUTPUT_FOLDER}
build=$root/build-web/src
pages=$root/deploy/web/site
p2pdc=${PEERCHALK_P2PDC_SOURCE_DIR:-$root/../p2p-discovery-connect}
math=$p2pdc/.deps/peerchalk-math-target/wasm32-unknown-emscripten/release

case $base in
    /*/ | /) ;;
    *) printf 'prepare-site.sh: BASE_PATH must start and end with /\n' >&2; exit 1 ;;
esac
if [ ! -f "$build/peerchalk.wasm" ]; then
    printf 'prepare-site.sh: build first with deploy/web/build.sh\n' >&2
    exit 1
fi

if [ ! -f "$math/peerchalk_math_worker.wasm" ]; then
    printf 'prepare-site.sh: build the math engine with deploy/web/build.sh\n' >&2
    exit 1
fi

mkdir -p "$out"
cp "$build/peerchalk.js" "$build/peerchalk.wasm" "$out/"
cp "$math/peerchalk_math_worker.js" "$math/peerchalk_math_worker.wasm" \
    "$root/src/math/engine/web/math-worker.js" "$out/"
cp -r "$pages/page.css" "$pages/privacy.html" "$pages/licenses" "$out/"
sed "s|@BASE@|$base|" "$pages/index.template.html" > "$out/index.html"
cp "$out/index.html" "$out/404.html"
touch "$out/.nojekyll" # serve every file exactly as it is
