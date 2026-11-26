A whiteboard that people share directly, computer to computer, with no account and no server
holding the boards. You can also [try it in the browser](https://brunoloff.github.io/peerchalk/).

## New in 0.4.0

- Every board has a connection button beside its name. Its dot shows at a glance whether
  you reach the others: green when you do (or nobody else has the board open), blue while
  PeerChalk is still finding them, red when something is wrong. It opens a pane that says
  in plain words what is going on and what to try, such as another network or turning off
  a VPN. The pane also shows who is on the board and how each person is connected, the
  servers PeerChalk uses, and a log you can copy or download. Phones have it too.
- Other web apps can now show a PeerChalk board inside their own pages, with the board's
  files kept in a git repository rather than in PeerChalk (docs/embedding.md). The VPvsVNP
  dashboard is the first to do so. Each release includes the browser build and the
  `peerchalk-board` program such apps download.
- Works with 0.3: both versions still draw together on the same boards.

## New in 0.3.0

- Every version, desktop and browser, now finds others through the DHT, Nostr relays, MQTT
  brokers and PeerJS servers at the same time, each with public servers ready to use. When
  one is blocked or down, the others still work.
- A new Network page in Settings: a live map of how this device reaches the others, directly
  or through a relay, with each one's round-trip time, beside lists of the servers in use.
  Each server shows whether it answers; add or remove servers with + and −, or reset to the
  defaults.
- Above your name in the sidebar, small chips show which ways of finding others are working.
  Click them to open the Network page.
- The version is shown on the welcome screen too.

## New in 0.2.3

- In the browser, strokes on a busy page no longer stop appearing after a few dozen. They
  were saved and shared all along; now they are drawn too.
- Browsers find each other through Nostr relays as well as the DHT, which helps on networks
  that block the DHT. Settings → Network → Discovery servers lists where they look.
- Wacom pens: hold the lower side button to pan; the eraser end or the upper side button
  erases.
- The version number is shown beside the name.

## New in 0.2.2

- Devices behind strict networks now reach each other through PeerChalk's own relay, with
  nothing to set up. To turn it off, set the TURN server to "none" in Settings → Network.
- New boards are page boards unless you choose the infinite board.

## New in 0.2.1

- PDFs and large boards reach the others on a board several times faster.
- A device that falls behind while receiving no longer drops its connection.

## New in 0.2

- **Page boards**, a second kind of board: columns of real-size pages (A4, Letter, A5, Legal,
  blank, ruled, squared or dotted), each column with its heading and subheadings, and
  "+ Page" and "+ Subheading" wherever you want them.
- **PDFs**: import a PDF as a column of its pages, or start a board from one, then write on
  them. They stay sharp at any zoom and travel to the others on the board.
- **Export** page by page at each page's exact size, a whole board, one column or a range of
  pages, with headings as bookmarks; PDF pages keep their original text.
- A layout for phones, Paper-style menus and dialogs, and the system's own file dialogs.
- Devices on 0.1 cannot connect to 0.2: update every device.

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
