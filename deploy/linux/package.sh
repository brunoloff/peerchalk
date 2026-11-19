#!/usr/bin/env bash
# Packages the Linux build in build/linux as an AppImage and a .tar.gz in dist/, for this
# computer's architecture. Build first with the deps and linux presets.
set -euo pipefail

readonly linuxdeploy_tag=1-alpha-20251107-1 plugin_tag=1-alpha-20250213-1
root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
readonly root arch=$(uname -m) tools=$root/.tools/linuxdeploy
version=$(sed -n 's/^project(PeerChalk VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")
readonly version name="peerchalk-$version-linux-$arch"

fetch() { # fetch REPOSITORY TAG FILE
    [ -x "$tools/$3" ] && return
    mkdir -p "$tools"
    curl -fsSL -o "$tools/$3" "https://github.com/linuxdeploy/$1/releases/download/$2/$3"
    chmod +x "$tools/$3"
}

qt_kit() {
    case $arch in
        x86_64) echo "$HOME/Qt/6.11.2/gcc_64" ;;
        aarch64) echo "$HOME/Qt/6.11.2/gcc_arm64" ;;
        *) printf 'package.sh: unsupported architecture %s\n' "$arch" >&2; exit 1 ;;
    esac
}

main() {
    fetch linuxdeploy "$linuxdeploy_tag" "linuxdeploy-$arch.AppImage"
    fetch linuxdeploy-plugin-qt "$plugin_tag" "linuxdeploy-plugin-qt-$arch.AppImage"
    fetch linuxdeploy-plugin-appimage "$plugin_tag" "linuxdeploy-plugin-appimage-$arch.AppImage"
    local appdir=$root/build/AppDir dist=$root/dist
    rm -rf "$appdir" "$dist/$name" && mkdir -p "$dist"
    DESTDIR=$appdir cmake --install "$root/build/linux" --prefix /usr
    export PATH=$tools:$PATH APPIMAGE_EXTRACT_AND_RUN=1 QMAKE="$(qt_kit)/bin/qmake"
    # Offscreen lets automated checks start the app without a display.
    export QML_SOURCES_PATHS=$root/src/ui EXTRA_PLATFORM_PLUGINS="libqwayland.so;libqoffscreen.so"
    export EXTRA_QT_MODULES=waylandcompositor LD_LIBRARY_PATH=$root/build/prefix-linux/lib:$(qt_kit)/lib
    export OUTPUT="$dist/$name.AppImage"
    (cd "$dist" && "linuxdeploy-$arch.AppImage" --appdir "$appdir" --plugin qt --output appimage \
        --desktop-file "$appdir/usr/share/applications/io.github.brunoloff.PeerChalk.desktop" \
        --icon-file "$appdir/usr/share/icons/hicolor/256x256/apps/io.github.brunoloff.PeerChalk.png")
    cp -a "$appdir" "$dist/$name"
    ln -s AppRun "$dist/$name/peerchalk"
    tar -C "$dist" -czf "$dist/$name.tar.gz" "$name"
    rm -rf "$dist/$name"
    (cd "$dist" && sha256sum "$name.AppImage" "$name.tar.gz")
}

main "$@"
