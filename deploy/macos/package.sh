#!/usr/bin/env bash
# Packages the macOS build in build/macos as PeerChalk.app inside a .dmg in dist/, for this
# Mac's architecture, and checks that nothing in it needs a macOS newer than 13 or a library
# outside the app. Build first with the deps-macos and macos presets.
set -euo pipefail

readonly minimum_macos=13.0
root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
readonly root qt=$HOME/Qt/6.11.2/macos arch=$(uname -m)
version=$(cat "$root/VERSION")
readonly version name="peerchalk-$version-macos-$arch"

fail() {
    printf 'package.sh: %s\n' "$*" >&2
    exit 1
}

# Every Mach-O file must target macOS 13 or older and load libraries only from the system or
# from inside the app.
check_bundle() {
    local app=$1 file minimum
    while IFS= read -r file; do
        minimum=$(vtool -show-build "$file" 2>/dev/null | awk '/minos/ {print $2; exit}')
        if [ -n "$minimum" ] && [ "$(printf '%s\n%s\n' "$minimum" "$minimum_macos" | sort -V | tail -1)" != "$minimum_macos" ]; then
            fail "$file needs macOS $minimum"
        fi
        if otool -L "$file" | tail -n +2 | grep -E '/opt/homebrew|/usr/local|/build/prefix-macos|/Qt/6' >/dev/null; then
            otool -L "$file" >&2
            fail "$file loads a library from outside the app"
        fi
    done < <(find "$app" -type f \( -perm -u+x -o -name '*.dylib' \) -exec sh -c 'file -b "$1" | grep -q Mach-O && echo "$1"' _ {} \;)
}

main() {
    local stage=$root/build/macos-app dist=$root/dist app
    rm -rf "$stage" && mkdir -p "$stage" "$dist"
    cmake --install "$root/build/macos" --prefix "$stage"
    app=$stage/PeerChalk.app
    DYLD_LIBRARY_PATH=$root/build/prefix-macos/lib "$qt/bin/macdeployqt" "$app" \
        -qmldir="$root/src/ui" -libpath="$root/build/prefix-macos/lib" -verbose=1
    # Qt's SQL drivers come along with its QML modules and load database libraries from outside
    # the app; PeerChalk uses none of them.
    rm -rf "$app/Contents/PlugIns/sqldrivers"
    codesign --force --deep --sign - "$app" # an ad-hoc signature, which Apple Silicon requires
    check_bundle "$app"
    make_dmg "$app" "$dist/$name.dmg"
    (cd "$dist" && shasum -a 256 "$name.dmg")
}

# hdiutil sometimes fails with "Resource busy" on build machines; a later try usually works.
make_dmg() { # app dmg
    local attempt
    for attempt in 1 2 3 4; do
        rm -f "$2"
        hdiutil create -volname PeerChalk -srcfolder "$1" -format UDZO "$2" && return 0
        printf 'hdiutil failed (attempt %s); trying again\n' "$attempt" >&2
        sleep $((attempt * 10))
    done
    return 1
}

main "$@"
