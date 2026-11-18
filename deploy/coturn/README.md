# A TURN relay for PeerChalk

PeerChalk connects devices directly. When both sides are behind strict NATs (common
on mobile and corporate networks) they need a TURN server to relay their traffic.
`setup-coturn.sh` turns a small Ubuntu server into one, using
[coturn](https://github.com/coturn/coturn).

## 1. Get a server

Any Ubuntu 22.04 or 24.04 VM with a public IPv4 address works. Oracle Cloud's Always
Free tier includes VMs and a large monthly outbound allowance (check Oracle's current
terms). Create the account and the VM yourself, then:

- In the Oracle console, open the VM's subnet → Security Lists → Add Ingress Rules,
  source `0.0.0.0/0`, for: UDP 3478, TCP 3478, and UDP 49160–49999.
- Note the VM's public IP address and SSH into it.

## 2. Run the script on the server

```sh
scp setup-coturn.sh ubuntu@SERVER:
ssh ubuntu@SERVER sudo ./setup-coturn.sh
```

It installs coturn, writes `/etc/turnserver.conf` (backing up an existing one), opens
the ports in the server's own firewall (Oracle's Ubuntu images block them by default)
and starts the service. Options: `--public-ip` if it cannot detect the address,
`--user` and `--password` to choose the credentials (otherwise a random password is
made), and `--print-config` to see the configuration without changing anything.

The relay refuses to forward to private and link-local addresses (including the cloud
metadata service), and limits sessions and bandwidth per session.

## 3. Use it in PeerChalk

The script ends by printing a line like `peerchalk:PASSWORD@203.0.113.7:3478`. Paste it
into Settings → Network → TURN server on each device and restart PeerChalk.

These credentials let anyone who has them relay traffic through your server, so share
them only with people you trust and never build them into the app. Serving every
PeerChalk user safely needs short-lived credentials handed out by a small service,
which is not built yet.

To change the password later, run the script again with `--password`, or edit the
`user=` line in `/etc/turnserver.conf` and run `sudo systemctl restart coturn`.
