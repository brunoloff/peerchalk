#pragma once

#include <QString>
#include <QStringList>

// The servers PeerChalk uses when the settings leave a list empty.
namespace peerchalk::sync::defaults {

// Jami's node, then PeerChalk's own on the relay's server (deploy/dht-proxy/).
inline QStringList bootstrap() {
    return {QStringLiteral("bootstrap.jami.net:4222"), QStringLiteral("89.168.35.4:4222")};
}

// Free public STUN servers from Cloudflare, with Google's as a fallback.
inline QStringList stun() {
    return {QStringLiteral("stun.cloudflare.com:3478"), QStringLiteral("stun.l.google.com:19302")};
}

// PeerChalk's open relay (deploy/coturn/), limited per IP address; its login is meant to be
// shared, like a public STUN server.
inline QStringList turn() {
    return {QStringLiteral("peerchalk:peerchalk-open-relay@89.168.35.4:3478")};
}

// Where devices meet besides the DHT (p2pdc's discovery backends), one entry each:
// Jami's DHT proxy, then PeerChalk's own (Jami's has resolved to a dead address in parts of
// Asia); two Nostr relays and HiveMQ's public MQTT broker, which work over WebSocket where
// streaming HTTP is blocked; and the public PeerJS server, which only relays WebRTC
// signalling. Desktops join the DHT themselves and skip the proxies.
inline QStringList discovery() {
    return {QStringLiteral("opendht https://dhtproxy.jami.net https://89-168-35-4.sslip.io"),
            QStringLiteral("nostr wss://nos.lol"), QStringLiteral("nostr wss://relay.primal.net"),
            QStringLiteral("mqtt wss://broker.hivemq.com:8884/mqtt"),
            QStringLiteral("peerjs wss://0.peerjs.com/peerjs peerjs")};
}

// The TURN setting that turns the relay off.
inline QString noTurn() { return QStringLiteral("none"); }

} // namespace peerchalk::sync::defaults
