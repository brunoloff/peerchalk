#!/usr/bin/env bash
# Packages the Windows build in build/windows as a folder with every DLL it needs, zipped, plus
# an installer, in dist/. Run in an MSYS2 UCRT64 shell after the deps-windows and windows presets.
set -euo pipefail

root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
readonly root prefix=$root/build/prefix-windows
version=$(cat "$root/VERSION")
readonly version name="peerchalk-$version-windows-x64"

# shellcheck source=SCRIPTDIR/libraries.sh
source "$root/deploy/windows/libraries.sh"

deploy_qt() {
    local tool
    tool=$(command -v windeployqt6 || command -v windeployqt-qt6 || command -v windeployqt)
    "$tool" --qmldir "$root/src/ui" --no-translations --release "$1/peerchalk.exe"
}

main() {
    local stage=$root/build/windows-app/$name dist=$root/dist
    rm -rf "$root/build/windows-app" && mkdir -p "$stage" "$dist"
    # qt_standard_project_setup puts Windows programs at the top of the build folder.
    cp "$root/build/windows/peerchalk.exe" "$stage/"
    mkdir -p "$stage/licenses"
    cp -r "$prefix/share/licenses/pdfium" "$stage/licenses/"
    deploy_qt "$stage"
    # MSYS2's Qt looks for QML modules in share/qt6/qml; windeployqt puts them in qml.
    printf '[Paths]\nPrefix = .\nPlugins = .\nQmlImports = qml\n' > "$stage/qt.conf"
    copy_libraries "$stage" "$prefix"
    check_libraries "$stage"
    (cd "$root/build/windows-app" && rm -f "$dist/$name.zip" && "/c/Program Files/7-Zip/7z.exe" a -tzip -bd "$dist/$name.zip" "$name" >/dev/null)
    # Every path is already in Windows form; MSYS2 would otherwise read /Q and /D... as paths.
    MSYS2_ARG_CONV_EXCL='*' "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" /Q "/DAppVersion=$version" "/DSourceDir=$(cygpath -w "$stage")" \
        "/DOutputDir=$(cygpath -w "$dist")" "/DOutputName=$name-setup" "$(cygpath -w "$root/deploy/windows/peerchalk.iss")"
    (cd "$dist" && sha256sum "$name.zip" "$name-setup.exe")
}

main "$@"
