#!/usr/bin/env python3
"""Regenerates deploy/web/site/licenses from the components the browser build links.

Run it after changing the Qt kit, Emscripten, libsodium or Yrs versions.
"""
from pathlib import Path
import json
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
P2PDC = ROOT.parent / "p2p-discovery-connect"
DEPS = P2PDC / ".deps"
QT = Path.home() / "Qt/6.11.2/wasm_singlethread"
QT_TEXTS = Path("/usr/share/licenses/qt6-base")
OUT = ROOT / "deploy/web/site/licenses"
EMSCRIPTEN = DEPS / "emsdk-qt/upstream/emscripten"
COPIES = {
    "PeerChalk-MIT.txt": ROOT / "LICENSE",
    "p2p-discovery-connect-MIT.txt": P2PDC / "LICENSE",
    "libsodium-ISC.txt": DEPS / "libsodium-1.0.22/LICENSE",
    "Yrs-MIT.txt": DEPS / "y-crdt-qt/LICENSE",
    "Emscripten.txt": EMSCRIPTEN / "LICENSE",
    "libc++-Apache-2.0-LLVM.txt": EMSCRIPTEN / "system/lib/libcxx/LICENSE.TXT",
    "musl-MIT.txt": EMSCRIPTEN / "system/lib/libc/musl/COPYRIGHT",
}
QT_LICENSES = ("LGPL-3.0-only", "GPL-3.0-only", "Apache-2.0", "BSD-2-Clause", "BSD-3-Clause",
               "BSL-1.0", "FTL", "IJG", "Libpng", "libpng-2.0", "MIT", "Zlib")


def qt_attributions() -> str:
    lines = ["Components included in Qt, from Qt's software bill of materials.\n"]
    for name in ("qtbase", "qtdeclarative", "qtsvg"):
        data = json.loads((QT / f"sbom/{name}-6.11.2.spdx.json").read_text())
        for package in sorted(data["packages"], key=lambda item: item["name"]):
            lines += [package["name"],
                      f"  License: {package.get('licenseConcluded', 'NOASSERTION')}",
                      f"  Copyright: {package.get('copyrightText', 'NOASSERTION')}", ""]
    return "\n".join(lines)


def rust_notices() -> str:
    environment = dict(os.environ, CARGO_HOME=str(DEPS / "cargo-reference"))
    tree = subprocess.run(
        ["cargo", "tree", "--offline", "--manifest-path", str(DEPS / "y-crdt-qt/Cargo.toml"),
         "-p", "yffi", "--target", "wasm32-unknown-emscripten", "-e", "normal", "--prefix", "none",
         "--format", "{p}|{l}"],
        env=environment, capture_output=True, text=True, check=True).stdout
    sources = next((DEPS / "cargo-reference/registry/src").iterdir())
    parts = []
    for line in sorted({entry.replace(" (*)", "") for entry in tree.splitlines()}):
        package, license_name = line.split("|")
        name, version = package.split()[:2]
        parts.append(f"=== {name} {version} ({license_name}) ===\n")
        folder = sources / f"{name}-{version[1:]}"
        for file in sorted(folder.glob("LICEN[CS]E*")) + sorted(folder.glob("COPYING*")):
            if file.is_file() and "APACHE" not in file.name.upper():
                parts.append(file.read_text(errors="replace").strip() + "\n")
    return "\n".join(parts)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name, source in COPIES.items():
        shutil.copyfile(source, OUT / name)
    for name in QT_LICENSES:
        shutil.copyfile(QT_TEXTS / f"{name}.txt", OUT / f"Qt-{name}.txt")
    (OUT / "Qt-components.txt").write_text(qt_attributions())
    (OUT / "Rust-crates.txt").write_text(rust_notices())


if __name__ == "__main__":
    main()
