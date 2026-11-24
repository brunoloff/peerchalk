# A DHT proxy for PeerChalk in the browser

Browsers cannot reach the DHT that PeerChalk uses to find peers, so the browser version goes
through an OpenDHT proxy over HTTPS. Jami's public proxy, `dhtproxy.jami.net`, is the
default; this sets up our own, on the same kind of server as the [TURN relay](../coturn/).

PeerChalk's runs at **https://89-168-35-4.sslip.io**, on the Oracle server that hosts the
relay. Its DHT node joins the same network as Jami's, so peers that use either proxy, or the
desktop app, find each other.

## Setting one up

1. In the Oracle console, add ingress rules to the VM's subnet security list from
   `0.0.0.0/0` for TCP 80 (certificates), TCP 443 (the proxy) and UDP 4222 (the DHT node).
2. On the server:

   ```sh
   scp setup-dht-proxy.sh ubuntu@SERVER:
   ssh ubuntu@SERVER sudo ./setup-dht-proxy.sh
   ```

It installs `dhtnode` with its proxy on local port 8080 and Caddy in front of it, which gets
a Let's Encrypt certificate for the server's sslip.io name (pass `--hostname` to use another
name that points at the server). Port 8080 stays closed to the outside.

The proxy's traffic counts towards the server's monthly outgoing traffic, which the relay's
[traffic cutoff](../coturn/README.md#traffic-cutoff) watches; DHT lookups are small next to
relayed whiteboards.
