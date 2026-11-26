#!/usr/bin/env bash
# Packages peerchalk-board, the program a dashboard runs for repository boards
# (docs/board-files.md), as dist/peerchalk-board-<version>-<os>-<arch>.zip: the program at the
# top with every library it loads that the system may lack, so it runs where it is unpacked.
# Then it runs the unpacked copy once, with no build folders in reach. Build first (the linux,
# macos or windows preset); on Windows, run in the MSYS2 UCRT64 shell.
#
#   deploy/board-tool/package.sh
set -euo pipefail

root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
version=$(cat "$root/VERSION")
readonly root version

fail() {
    printf 'package.sh: %s\n' "$*" >&2
    exit 1
}

# The system's own libraries, which every Linux has and which must not be bundled.
linux_system_library() {
    case $1 in
        linux-vdso.so* | ld-linux*.so* | libc.so* | libm.so* | libdl.so* | libpthread.so* | \
            librt.so* | libgcc_s.so* | libstdc++.so* | libresolv.so* | libutil.so*) return 0 ;;
        *) return 1 ;;
    esac
}

# Linux: peerchalk-board and lib/, each library finding the others beside it.
bundle_linux() { # stage
    local stage=$1 path name
    local qt=$HOME/Qt/6.11.2/gcc_64
    [ "$(uname -m)" = aarch64 ] && qt=$HOME/Qt/6.11.2/gcc_arm64
    mkdir -p "$stage/lib"
    cp "$root/build/linux/src/peerchalk-board" "$stage/"
    while read -r name path; do
        name=$(basename "$name") # the loader is listed by its path
        linux_system_library "$name" && continue
        [ -n "$path" ] || fail "peerchalk-board needs $name, which was not found"
        cp -L "$path" "$stage/lib/$name"
        chmod u+w "$stage/lib/$name"
        # shellcheck disable=SC2016 # $ORIGIN is for the loader, not the shell
        patchelf --set-rpath '$ORIGIN' "$stage/lib/$name"
    done < <(LD_LIBRARY_PATH=$root/build/prefix-linux/lib:$qt/lib ldd "$stage/peerchalk-board" |
        awk '$2 == "=>" {print $1, $3}')
    # DT_RPATH rather than RUNPATH: it also applies to the libraries' own dependencies.
    # shellcheck disable=SC2016 # $ORIGIN is for the loader, not the shell
    patchelf --force-rpath --set-rpath '$ORIGIN/lib' "$stage/peerchalk-board"
}

# macOS: where a library a Mach-O file names is found when it is built.
resolve_macos() { # reference
    local reference=$1 base
    case $reference in
        /*) [ -e "$reference" ] && { echo "$reference"; return; } ;;
        @rpath/* | @loader_path/* | @executable_path/*)
            reference=${reference#*/}
            for base in "$HOME/Qt/6.11.2/macos/lib" "$root/build/prefix-macos/lib"; do
                [ -e "$base/$reference" ] && { echo "$base/$reference"; return; }
            done ;;
    esac
    fail "peerchalk-board needs $1, which was not found"
}

# The libraries a Mach-O file loads from outside the system, without its own name.
macos_dependencies() { # file
    local own
    own=$(otool -D "$1" | tail -n +2)
    otool -L "$1" | tail -n +2 | awk '{print $1}' | grep -vE '^(/usr/lib|/System)/' |
        grep -vxF "${own:-//none}" || true
}

# macOS: peerchalk-board and lib/ with every library flattened into it (Qt's framework as a
# plain file), each named @rpath/<file>; then an ad-hoc signature, which Apple Silicon needs.
bundle_macos() { # stage
    local stage=$1 file reference name
    local queue=("$stage/peerchalk-board")
    mkdir -p "$stage/lib"
    cp "$root/build/macos/src/peerchalk-board" "$stage/"
    install_name_tool -add_rpath @executable_path/lib "$stage/peerchalk-board"
    while [ ${#queue[@]} -gt 0 ]; do
        file=${queue[0]}
        queue=("${queue[@]:1}")
        while read -r reference; do
            [ -n "$reference" ] || continue
            name=$(basename "$reference")
            if [ ! -e "$stage/lib/$name" ]; then
                cp -L "$(resolve_macos "$reference")" "$stage/lib/$name"
                chmod u+w "$stage/lib/$name"
                install_name_tool -id "@rpath/$name" "$stage/lib/$name"
                queue+=("$stage/lib/$name")
            fi
            install_name_tool -change "$reference" "@rpath/$name" "$file"
        done < <(macos_dependencies "$file")
    done
    codesign --force --sign - "$stage"/lib/* "$stage/peerchalk-board"
    if otool -L "$stage/peerchalk-board" "$stage"/lib/* | grep -E '/opt/homebrew|/usr/local|prefix-macos|/Qt/6'; then
        fail "a library still points outside the package"
    fi
}

# Windows: peerchalk-board.exe with the DLLs beside it, as the app's package has them.
bundle_windows() { # stage
    local stage=$1
    # shellcheck source=SCRIPTDIR/../windows/libraries.sh
    source "$root/deploy/windows/libraries.sh"
    # qt_standard_project_setup puts Windows programs at the top of the build folder.
    cp "$root/build/windows/peerchalk-board.exe" "$stage/"
    copy_libraries "$stage" "$root/build/prefix-windows"
    check_libraries "$stage"
}

platform() {
    case $(uname -s) in
        Linux) echo linux ;;
        Darwin) echo macos ;;
        MINGW* | MSYS*) echo windows ;;
        *) fail "unsupported system $(uname -s)" ;;
    esac
}

# Runs the unpacked package once, asking for a new identity in a fresh folder.
check_package() { # zip
    local where answer
    where=$(mktemp -d)
    python3 -m zipfile -e "$1" "$where/package"
    local program=$where/package/peerchalk-board
    [ -e "$program.exe" ] && program=$program.exe
    chmod +x "$program" # python's zipfile does not restore modes
    answer=$(env -u LD_LIBRARY_PATH -u DYLD_LIBRARY_PATH "$program" --user-dir "$where/user" identity)
    case $answer in
        *'"userKey"'*) printf 'The package runs: %s\n' "$answer" ;;
        *) fail "the unpacked peerchalk-board answered: $answer" ;;
    esac
    rm -rf "$where"
}

main() {
    local system arch stage name
    system=$(platform)
    arch=$(uname -m)
    case $arch in amd64 | x86_64) arch=x64 ;; aarch64) arch=arm64 ;; esac
    name="peerchalk-board-$version-$system-$arch"
    stage=$root/build/board-tool/$name
    rm -rf "$stage" && mkdir -p "$stage" "$root/dist"
    "bundle_$system" "$stage"
    rm -f "$root/dist/$name.zip"
    # Python keeps each file's mode in the archive; run from the stage, the files are at its top.
    (cd "$stage" && python3 -m zipfile -c "$root/dist/$name.zip" ./*)
    check_package "$root/dist/$name.zip"
    if command -v sha256sum >/dev/null; then
        (cd "$root/dist" && sha256sum "$name.zip")
    else
        (cd "$root/dist" && shasum -a 256 "$name.zip")
    fi
}

main "$@"
