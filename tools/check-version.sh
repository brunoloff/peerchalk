#!/usr/bin/env bash
# Checks that a release says the same version everywhere it must be written by hand: VERSION
# names it; the newest release in the AppStream metadata and the release notes must match, and
# so must the tag when one is given (tools/check-version.sh v0.2.3).
set -euo pipefail

root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
version=$(cat "$root/VERSION")
problems=0

fail() {
    echo "::error::$1" >&2
    problems=$((problems + 1))
}

[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "VERSION holds \"$version\", not X.Y.Z"
newest=$(grep -m1 -o '<release version="[^"]*"' "$root/deploy/linux/io.github.brunoloff.PeerChalk.metainfo.xml" |
    sed 's/.*"\(.*\)"/\1/')
[[ "$newest" == "$version" ]] || fail "the metainfo's newest release is $newest, not $version"
grep -q "^## New in $version\$" "$root/deploy/release-notes.md" ||
    fail "deploy/release-notes.md has no \"## New in $version\" section"
if [[ $# -gt 0 && "$1" != "v$version" ]]; then
    fail "the tag $1 does not name version $version"
fi
[[ $problems -eq 0 ]] && echo "Version $version is consistent"
exit "$problems"
