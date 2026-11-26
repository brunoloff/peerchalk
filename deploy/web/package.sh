#!/usr/bin/env bash
# Zips the browser build for apps that embed PeerChalk (docs/embedding.md), such as the
# VPvsVNP dashboard: dist/peerchalk-web-<version>.zip, the site at the archive's top, made
# for the base path /peerchalk/, where such apps serve it. Build first with
# deploy/web/build.sh /peerchalk/.
set -euo pipefail

root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
readonly root site=$root/build/web/site
version=$(cat "$root/VERSION")
readonly version name="peerchalk-web-$version.zip"

if ! grep -q '<base href="/peerchalk/">' "$site/index.html" 2>/dev/null; then
    printf 'package.sh: build the site for /peerchalk/ first (deploy/web/build.sh /peerchalk/)\n' >&2
    exit 1
fi
mkdir -p "$root/dist"
rm -f "$root/dist/$name"
(cd "$site" && python3 -m zipfile -c "$root/dist/$name" ./*)
(cd "$root/dist" && sha256sum "$name")
