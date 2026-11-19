# Flatpak

`io.github.brunoloff.PeerChalk.yml` builds PeerChalk on the KDE 6.11 runtime without network
access during the build, as Flathub requires. Rust crates come from the two source lists made
by `cargo_sources.py`; make them again when a `Cargo.lock` changes:

```sh
deploy/flatpak/cargo_sources.py src/math/engine/Cargo.lock > deploy/flatpak/math-cargo-sources.json
deploy/flatpak/cargo_sources.py <y-crdt v0.28.0>/Cargo.lock > deploy/flatpak/yrs-cargo-sources.json
```

The release workflow builds the manifest from the checked-out commit and starts the result.
For Flathub, the PeerChalk source in the manifest must name a release tag and its commit.
