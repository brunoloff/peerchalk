#!/usr/bin/env python3
"""Regenerates deploy/web/site/licenses from the components the browser build links.

Run it after changing the Qt kit, Emscripten, libsodium, Yrs, PDFium or math engine versions.
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
PDFIUM_LICENSES = ROOT / "build/prefix-web/share/licenses/pdfium"
EMSCRIPTEN = DEPS / "emsdk-qt/upstream/emscripten"
COPIES = {
    "PeerChalk-MIT.txt": ROOT / "LICENSE",
    "p2p-discovery-connect-MIT.txt": P2PDC / "LICENSE",
    "libsodium-ISC.txt": DEPS / "libsodium-1.0.22/LICENSE",
    "Yrs-MIT.txt": DEPS / "y-crdt-qt/LICENSE",
    "Emscripten.txt": EMSCRIPTEN / "LICENSE",
    "libc++-Apache-2.0-LLVM.txt": EMSCRIPTEN / "system/lib/libcxx/LICENSE.TXT",
    "musl-MIT.txt": EMSCRIPTEN / "system/lib/libc/musl/COPYRIGHT",
    "RaTeX-MIT.txt": DEPS / "ratex-src/LICENSE",
    "RaTeX-third-party.txt": DEPS / "ratex-src/THIRD_PARTY_NOTICES.txt",
    "KaTeX-fonts-OFL-1.1.txt": DEPS / "ratex-src/licenses/SIL-OFL-1.1.txt",
    "KaTeX-fonts-notice.txt": DEPS / "ratex-src/licenses/KaTeX-fonts-NOTICE.txt",
}
# License texts of crates that do not ship their own, kept beside this script.
BUNDLED_CRATE_LICENSES = {
    "lyon_geom": "lyon-MIT.txt", "lyon_path": "lyon-MIT.txt", "lyon_tessellation": "lyon-MIT.txt",
}
RATEX_CRATES = "ratex-"
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


def crate_license_text(name: str, folder: Path) -> list[str]:
    files = sorted(folder.glob("LICEN[CS]E*")) + sorted(folder.glob("COPYING*"))
    texts = [file.read_text(errors="replace").strip() + "\n" for file in files
             if file.is_file() and "APACHE" not in file.name.upper()]
    if texts:
        return texts
    if name.startswith(RATEX_CRATES):
        return ["See RaTeX-MIT.txt.\n"]
    bundled = BUNDLED_CRATE_LICENSES.get(name)
    if bundled is None:
        raise SystemExit(f"no license text found for the crate {name}")
    return [(Path(__file__).parent / "notices" / bundled).read_text().strip() + "\n"]


def rust_notices(manifest: Path, package: str) -> str:
    environment = dict(os.environ, CARGO_HOME=str(DEPS / "cargo-reference"))
    tree = subprocess.run(
        ["cargo", "tree", "--offline", "--manifest-path", str(manifest), "-p", package,
         "--target", "wasm32-unknown-emscripten", "-e", "normal", "--prefix", "none",
         "--format", "{p}|{l}"],
        env=environment, capture_output=True, text=True, check=True).stdout
    sources = next((DEPS / "cargo-reference/registry/src").iterdir())
    parts = []
    for line in sorted({entry.replace(" (*)", "") for entry in tree.splitlines()}):
        package_id, license_name = line.split("|")
        name, version = package_id.split()[:2]
        folder = sources / f"{name}-{version[1:]}"
        if not folder.is_dir():
            continue # the workspace's own crates
        parts.append(f"=== {name} {version} ({license_name}) ===\n")
        parts += crate_license_text(name, folder)
    return "\n".join(parts)


def pdfium_notices() -> str:
    """The licenses of the libraries inside PDFium, one after another."""
    parts = []
    for path in sorted(PDFIUM_LICENSES.iterdir()):
        if path.name != "pdfium.txt":
            parts.append(f"==== {path.stem} ====\n\n{path.read_text(errors='replace').strip()}\n")
    return "\n".join(parts)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name, source in COPIES.items():
        shutil.copyfile(source, OUT / name)
    for name in QT_LICENSES:
        shutil.copyfile(QT_TEXTS / f"{name}.txt", OUT / f"Qt-{name}.txt")
    (OUT / "Qt-components.txt").write_text(qt_attributions())
    (OUT / "Rust-crates.txt").write_text(rust_notices(DEPS / "y-crdt-qt/Cargo.toml", "yffi"))
    (OUT / "Math-Rust-crates.txt").write_text(
        rust_notices(ROOT / "src/math/engine/Cargo.toml", "peerchalk-math"))
    shutil.copyfile(PDFIUM_LICENSES / "pdfium.txt", OUT / "PDFium-BSD-3-Clause.txt")
    (OUT / "PDFium-third-party.txt").write_text(pdfium_notices())


if __name__ == "__main__":
    main()
