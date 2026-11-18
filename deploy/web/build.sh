#!/usr/bin/env bash
# Builds PeerChalk for browsers into build/web/site. Run tools/setup.sh web once first.
# Set CMAKE_BUILD_PARALLEL_LEVEL=1 to keep the computer responsive meanwhile.
set -euo pipefail

root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$root"
export EMSDK_QUIET=1
# shellcheck disable=SC1091
source .emsdk/emsdk_env.sh
[ -d build/prefix-web/math-worker ] || cmake --workflow --preset deps-web
cmake --workflow --preset web
rm -rf build/web/site
deploy/web/prepare-site.sh "${1:-/}" build/web/site
