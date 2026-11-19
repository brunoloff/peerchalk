A whiteboard that people share directly, computer to computer, with no account and no server
holding the boards. You can also [try it in the browser](https://brunoloff.github.io/peerchalk/).

## Which file to download

| System | File |
|---|---|
| Windows 10 or 11 (64-bit) | `peerchalk-@VERSION@-windows-x64-setup.exe` (installer) or the `.zip` |
| macOS 13 or later, Apple silicon | `peerchalk-@VERSION@-macos-arm64.dmg` |
| macOS 13 or later, Intel | `peerchalk-@VERSION@-macos-x86_64.dmg` |
| Linux, any distribution | the `.AppImage` for your processor (`x86_64` or `aarch64`) |
| Linux with Flatpak | the `.flatpak` for your processor |
| Linux, a plain folder | the `-linux-` `.tar.gz` for your processor; run `peerchalk` inside it |
| Arch Linux | the AUR packages `peerchalk` or `peerchalk-bin` |

`SHA256SUMS` lists each file's checksum.

## Good to know

- The Windows and macOS apps are not signed yet. Windows may show "Windows protected your PC":
  choose **More info**, then **Run anyway**. On macOS, open the app with right-click, **Open**,
  then confirm.
- The `aarch64` Linux builds need glibc 2.39 or newer, such as Ubuntu 24.04 or Debian 13.
