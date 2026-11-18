# Public release requirements

What PeerChalk needs before it is offered to the public. Items marked **Decided** are
product rules; the rest is work identified by reviewing the code.

## Accounts and backups

- The first screen chooses an account: pick one on this device, create one, link with
  another device, or import a backup.
- Creating an account says clearly that it is local, that no server stores it, and that
  the user is responsible for backing it up.
- Users can sign out and back in, and remove an account from a device.
- Encrypted export and import of everything, of identity and known people, or of known
  people alone. Automatic backups come later.
- Linking must stop exposing the identity secret as copyable text.
- Decide how a lost device is handled: every device currently shares one secret, so one
  device cannot be revoked. Settle this before accounts exist in the wild.

## Board links (Decided)

The app will be hosted on GitHub Pages, and a board can be shared as a single link:

    https://brunoloff.github.io/peerchalk/board/<board-link>

- Opening the link is enough to join the board, with no separate invite or approval step
  for the person who received it.
- **Anyone who has joined can open the board at any time and see it as their device last
  had it.**
- **Members can edit, and receive live changes, only while an owner of the board is
  online and connected.** Without one, the board is read-only and says why.

How it works:

- The link is `/board/<id>#<secret>`. Browsers do not send the part after `#` to the host,
  and the app removes it from the address bar once used.
- The signed access log holds only a hash of the link's token, and owners can replace the
  link or turn it off. The token goes only to verified owners. People who were removed from
  a board are not readmitted by its link.
- The first visit needs an owner online, to admit the person and hand them the board.
- Board contents flow only between owners and others, never member to member, so nothing
  changes hands without an owner. A modified app can still submit edits it made offline,
  but they take effect only when an owner is online, as if made at that moment.
- `deploy/web/prepare-site.sh /peerchalk/ <folder>` makes the static site, with a `404.html`
  that starts the app on `/board/...` addresses. Publishing it is still to do.

The site uses its own start page and loader instead of Qt's, and includes licenses and a
privacy note. Any number of tabs and windows can run at once, each with its own storage
slot and device keys, keeping each other up to date directly.

Still open: GitHub Pages cannot set security headers, so a content security policy must
come from a `<meta>` tag. Changing a board's discovery secret when a link is replaced is
also still to do.

## Sync and compatibility

- Done: large messages travel in parts, peers negotiate protocol versions and ignore
  message types they do not know, reconnecting sends only what the other side lacks, and
  more than 16 projects and boards take turns being online.
- With more than 16, a waiting item can take many minutes to get its turn, because p2pdc
  admits 16 announcements per 5 minutes. Check whether this is good enough in real use.
- Decide what happens when the people sharing a board are never online together.

## Infrastructure

- Run our own OpenDHT bootstrap nodes and at least two HTTPS proxies, with browser failover.
- Run an open TURN relay on an Oracle Always Free server (**Decided**). Its credentials ship
  in the app, like a public STUN server, so nobody needs an account or a pass. Limits make it
  ample for whiteboards and poor for anything else:
  - each IP address, in each direction: 40 KB/s, an 8 MB burst for loading boards, and 1 GB
    per day; at most 32 connections per address;
  - the whole server: 3.5 MB/s, about 9 TB per 30 days, inside the 10 TB monthly allowance.
- These limits discourage video and bulk traffic but cannot rule them out: very low-quality
  video fits in 40 KB/s, and someone can spread across addresses. Watch real use, and consider
  short-lived credentials from a small service if the relay is abused.
- Someone drawing to several people through the relay can reach 40 KB/s. If that hurts in
  practice, make live strokes more compact rather than raising the limit.
- Add TURN over TLS on port 443 (needs a domain name and certificate), since strict networks
  often block port 3478.
- Let one instance on a device do discovery for the others. Today every open tab or window
  polls the DHT proxy on its own, which multiplies the load.

## Security

- Finish p2pdc's protocol freeze, adversarial testing and load limits, then commission an
  independent audit.
- Protect keys at rest with the OS keychain or a passphrase.
- Add board version history, since any member can erase strokes.
- Change a project's or board's discovery secret when someone is removed.
- Harden the hosted site: no third-party scripts, a content security policy and verifiable
  builds. Request persistent browser storage and prevent two tabs from overwriting data.

## Product and release

- PNG/PDF export, clear sync status, blocking people, limits on join-request spam, and a
  warning that display names are not verified.
- Signed installers, an update path, opt-in crash reports, accessibility, and testing on
  Safari and phones.
- Legal review: privacy policy (peers and network services see IP addresses), terms, use
  of third-party network services, and Qt's LGPL terms on static targets.
