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

case $base in
    /*/ | /) ;;
    *) printf 'prepare-site.sh: BASE_PATH must start and end with /\n' >&2; exit 1 ;;
esac
if [ ! -f "$build/peerchalk.wasm" ]; then
    printf 'prepare-site.sh: build first with deploy/web/build.sh\n' >&2
    exit 1
fi

mkdir -p "$out"
cp "$build/peerchalk.js" "$build/peerchalk.wasm" "$build/qtloader.js" "$build/qtlogo.svg" "$out/"
sed -e "s|<head>|<head>\n    <base href=\"$base\">|" \
    -e 's|<title>peerchalk</title>|<title>PeerChalk</title>|' \
    -e 's|Qt for WebAssembly: peerchalk|PeerChalk|' \
    "$build/peerchalk.html" > "$out/index.html"
cp "$out/index.html" "$out/404.html"
touch "$out/.nojekyll" # serve every file exactly as it is
