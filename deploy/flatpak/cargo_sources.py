#!/usr/bin/env python3
"""Lists a Cargo.lock's crates as Flatpak sources, so that Flathub can build without internet.

    deploy/flatpak/cargo_sources.py Cargo.lock > sources.json

Each crate from crates.io is downloaded by checksum into cargo/vendor, and a cargo/config.toml
points Cargo there. Run it again whenever the Cargo.lock changes.
"""
import json
import sys
import tomllib

CRATES = "https://static.crates.io/crates/{name}/{name}-{version}.crate"
REGISTRY = "registry+https://github.com/rust-lang/crates.io-index"


def crate_sources(package):
    name, version = package["name"], package["version"]
    folder = f"cargo/vendor/{name}-{version}"
    checksum = json.dumps({"package": package["checksum"], "files": {}})
    return [
        {"type": "archive", "archive-type": "tar-gzip", "url": CRATES.format(name=name, version=version),
         "sha256": package["checksum"], "dest": folder},
        {"type": "inline", "contents": checksum, "dest": folder, "dest-filename": ".cargo-checksum.json"},
    ]


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    with open(sys.argv[1], "rb") as lock:
        packages = tomllib.load(lock)["package"]
    unsupported = [p["name"] for p in packages if "source" in p and p["source"] != REGISTRY]
    if unsupported:
        sys.exit(f"cargo_sources.py: only crates.io sources are supported, not {unsupported}")
    sources = [source for p in packages if p.get("source") == REGISTRY for source in crate_sources(p)]
    sources.append({"type": "inline", "dest": "cargo", "dest-filename": "config.toml", "contents":
                    '[source.crates-io]\nreplace-with = "vendored-sources"\n\n'
                    '[source.vendored-sources]\ndirectory = "cargo/vendor"\n'})
    json.dump(sources, sys.stdout, indent=1)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
