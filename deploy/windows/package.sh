#!/usr/bin/env bash
# Packages the Windows build in build/windows as a folder with every DLL it needs, zipped, plus
# an installer, in dist/. Run in an MSYS2 UCRT64 shell after the deps-windows and windows presets.
set -euo pipefail

root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
readonly root prefix=$root/build/prefix-windows
version=$(sed -n 's/^project(PeerChalk VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")
readonly version name="peerchalk-$version-windows-x64"

deploy_qt() {
    local tool
    tool=$(command -v windeployqt6 || command -v windeployqt-qt6 || command -v windeployqt)
    "$tool" --qmldir "$root/src/ui" --no-translations --release "$1/peerchalk.exe"
}

# Copies the DLLs that the program and its DLLs load from MSYS2 or the dependency prefix, until
# nothing new turns up. Windows' own DLLs stay out.
copy_libraries() {
    local folder=$1 added=1 dll
    while [ "$added" -gt 0 ]; do
        added=0
        while IFS= read -r dll; do
            [ -e "$folder/$(basename "$dll")" ] && continue
            cp "$dll" "$folder/"
            added=$((added + 1))
        done < <(find "$folder" -name '*.exe' -o -name '*.dll' | while read -r file; do
            PATH="$prefix/bin:$PATH" ldd "$file" | awk '{print $3}' |
                grep -iE '^(/ucrt64/bin|'"$prefix"'/bin)/' || true
        done | sort -u)
    done
}

# Fails when a program or DLL in the package needs a DLL that is neither in the package nor part
# of Windows, which Windows reports in a dialog rather than an error code. Windows' own DLLs are
# not followed: some of what they name exists only on some editions.
check_libraries() {
    local ntldd missing
    ntldd=$(command -v ntldd)
    missing=$(cd "$1" && find . -name '*.exe' -o -name '*.dll' | while read -r file; do
        PATH="$PWD:$(cygpath -u "$SYSTEMROOT")/System32" "$ntldd" "$file" | grep -i 'not found' |
            grep -viE '(api|ext)-ms-' | sed "s|^|$file: |" || true # API sets are not files
    done)
    if [ -n "$missing" ]; then
        printf 'package.sh: the package needs DLLs that are not in it:\n%s\n' "$missing" >&2
        exit 1
    fi
}

main() {
    local stage=$root/build/windows-app/$name dist=$root/dist
    rm -rf "$root/build/windows-app" && mkdir -p "$stage" "$dist"
    # qt_standard_project_setup puts Windows programs at the top of the build folder.
    cp "$root/build/windows/peerchalk.exe" "$stage/"
    deploy_qt "$stage"
    # MSYS2's Qt looks for QML modules in share/qt6/qml; windeployqt puts them in qml.
    printf '[Paths]\nPrefix = .\nPlugins = .\nQmlImports = qml\n' > "$stage/qt.conf"
    copy_libraries "$stage"
    check_libraries "$stage"
    (cd "$root/build/windows-app" && rm -f "$dist/$name.zip" && "/c/Program Files/7-Zip/7z.exe" a -tzip -bd "$dist/$name.zip" "$name" >/dev/null)
    # Every path is already in Windows form; MSYS2 would otherwise read /Q and /D... as paths.
    MSYS2_ARG_CONV_EXCL='*' "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" /Q "/DAppVersion=$version" "/DSourceDir=$(cygpath -w "$stage")" \
        "/DOutputDir=$(cygpath -w "$dist")" "/DOutputName=$name-setup" "$(cygpath -w "$root/deploy/windows/peerchalk.iss")"
    (cd "$dist" && sha256sum "$name.zip" "$name-setup.exe")
}

main "$@"
