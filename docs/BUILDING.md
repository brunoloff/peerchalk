# Building PeerChalk

These steps start from a clean checkout. Nothing from other projects is stored in this
repository: the build downloads everything at pinned versions. GitHub Actions runs the same
commands on Ubuntu 24.04 for every push (see `.github/workflows/linux.yml`).

## What you install yourself

CMake 3.25 or newer, Ninja, Git, a C++20 compiler, pkg-config, Meson, Python 3, rustup, and the
development files listed below.

On Ubuntu 24.04:

```sh
sudo apt-get install build-essential cmake ninja-build git pkg-config meson python3-venv \
  libglib2.0-dev libssl-dev libgnutls28-dev nettle-dev libargon2-dev libfmt-dev libgtest-dev \
  libgmock-dev libgl1-mesa-dev libegl1 libxkbcommon-dev libvulkan-dev libdbus-1-3 libfontconfig1 libfreetype6
```

On Arch Linux or Manjaro:

```sh
sudo pacman -S --needed base-devel cmake ninja git pkgconf meson python rustup glib2 openssl \
  gnutls nettle argon2 fmt gtest
```

## Linux

```sh
git clone https://github.com/brunoloff/peerchalk.git
cd peerchalk
tools/setup.sh desktop
cmake --workflow --preset deps
cmake --workflow --preset linux
bin/peerchalk
```

- `tools/setup.sh` installs Rust 1.96.1 and, using aqtinstall, Qt 6.11.2 into `~/Qt`.
- The `deps` preset downloads p2p-discovery-connect at the commit in
  `cmake/P2pdcSource.cmake`, then builds its dependencies, Yrs and the math engine into
  `build/prefix-linux`, and adds PDFium, prebuilt by
  [pdfium-binaries](https://github.com/bblanchon/pdfium-binaries) at one pinned release for
  every platform. Run it again when `deps/`, `src/math/engine/` or that commit changes.
- Browsers use the same PDFium: the page loads its WebAssembly module beside the app, and
  `src/pdf/pdfium_web_bridge.cpp` forwards the PDFium calls to it. After building for
  browsers, `node tests/web/pdf_bridge_test.mjs` and `node tests/web/run_pdf_web_check.mjs`
  (with Emscripten's environment loaded) check that bridge.
- The `linux` preset builds the app and runs its tests.

To keep the computer responsive during builds, limit the work done at once:

```sh
export CMAKE_BUILD_PARALLEL_LEVEL=1
```

## Browsers

```sh
tools/setup.sh web
deploy/web/build.sh
bin/peerchalk-web
```

This adds Qt for WebAssembly and Emscripten 4.0.7, builds the app and assembles the site in
`build/web/site`. To build for a site in a subfolder, pass its path, such as
`deploy/web/build.sh /peerchalk/`.

## Working on p2p-discovery-connect at the same time

Point the build at your checkout when configuring:

```sh
cmake --preset linux -DFETCHCONTENT_SOURCE_DIR_P2PDC=/path/to/p2pdc
```

## License notices

`deploy/web/generate-notices.py` regenerates the notices in `deploy/web/site/licenses` after a
dependency changes. It reads the sources kept in a local p2p-discovery-connect `.deps` folder.
