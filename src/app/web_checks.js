// Server checks for the settings page in browsers (server_checks_web.cpp). Browsers cannot
// open TLS or UDP sockets, so a web server is checked with a WebSocket or an HTTP request
// and a STUN or TURN server by gathering a WebRTC candidate that only it can give.

const peerchalkCheckTimeoutMs = 8000;

function peerchalkCheckServer(id, kind, address, login) {
  const started = performance.now();
  let done = false;
  const finish = (working) => {
    if (done)
      return;
    done = true;
    _peerchalk_server_checked(id, working ? 1 : 0, Math.round(performance.now() - started));
  };
  setTimeout(() => finish(false), peerchalkCheckTimeoutMs);
  try {
    if (kind === "stun" || kind === "turn")
      return peerchalkCheckIce(kind, address, login, finish);
    const [url, key] = address.split(" ");
    if (kind === "peerjs")
      return peerchalkCheckFetch(`${url.replace(/^ws/, "http").replace(/\/$/, "")}/${key || "peerjs"}/id`, finish);
    if (url.startsWith("https://"))
      return peerchalkCheckFetch(url, finish);
    const socket = kind === "mqtt" ? new WebSocket(url, "mqtt") : new WebSocket(url);
    socket.onopen = () => {
      finish(true);
      socket.close();
    };
    socket.onerror = () => finish(false);
  } catch (error) {
    console.warn(`PeerChalk could not check ${address}:`, error);
    finish(false);
  }
}

// Any answer, even an error page, shows the server is reachable; no-cors keeps it opaque.
function peerchalkCheckFetch(url, finish) {
  fetch(url, { mode: "no-cors", cache: "no-store" }).then(() => finish(true), () => finish(false));
}

// A STUN server gives a server-reflexive candidate, a TURN server a relayed one.
function peerchalkCheckIce(kind, address, login, finish) {
  const server = { urls: `${kind}:${address}` };
  if (kind === "turn") {
    const colon = login.indexOf(":");
    server.username = login.slice(0, colon);
    server.credential = login.slice(colon + 1);
  }
  const connection = new RTCPeerConnection({
    iceServers: [server],
    iceTransportPolicy: kind === "turn" ? "relay" : "all",
  });
  const wanted = kind === "turn" ? " typ relay" : " typ srflx";
  connection.onicecandidate = (event) => {
    if (event.candidate && event.candidate.candidate.includes(wanted)) {
      finish(true);
      connection.close();
    }
  };
  connection.createDataChannel("check");
  connection.createOffer()
    .then((offer) => connection.setLocalDescription(offer))
    .catch(() => finish(false));
  setTimeout(() => connection.close(), peerchalkCheckTimeoutMs);
}
