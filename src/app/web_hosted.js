// Linked into the browser build with --pre-js. When the page is opened with ?hosted=1 inside
// a dashboard's frame, this is its link to the dashboard (docs/board-files.md, "Hosted
// mode"): messages from the parent page, of the same origin only, wait in a queue for the
// app, and the app's messages go to the parent. Uint8Arrays travel to and from the app as
// {"$bytes": base64}.
(() => {
  if (new URLSearchParams(location.search).get("hosted") !== "1" || window.parent === window) {
    return;
  }
  const inbox = [];
  const chunk = 0x8000;

  function toBase64(bytes) {
    let text = "";
    for (let i = 0; i < bytes.length; i += chunk) {
      text += String.fromCharCode.apply(null, bytes.subarray(i, i + chunk));
    }
    return btoa(text);
  }

  function fromBase64(text) {
    const binary = atob(text);
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i += 1) bytes[i] = binary.charCodeAt(i);
    return bytes;
  }

  function encode(value) {
    if (value instanceof Uint8Array) return { $bytes: toBase64(value) };
    if (Array.isArray(value)) return value.map(encode);
    if (value && typeof value === "object") {
      return Object.fromEntries(Object.entries(value).map(([key, item]) => [key, encode(item)]));
    }
    return value;
  }

  function decode(value) {
    if (Array.isArray(value)) return value.map(decode);
    if (value && typeof value === "object") {
      if (typeof value.$bytes === "string" && Object.keys(value).length === 1) {
        return fromBase64(value.$bytes);
      }
      return Object.fromEntries(Object.entries(value).map(([key, item]) => [key, decode(item)]));
    }
    return value;
  }

  addEventListener("message", (event) => {
    if (event.source !== window.parent || event.origin !== location.origin) return;
    if (!event.data || typeof event.data.type !== "string") return;
    inbox.push(JSON.stringify(encode(event.data)));
  });

  globalThis.peerchalkHosted = {
    next: () => (inbox.length ? inbox.shift() : ""),
    post: (text) => window.parent.postMessage(decode(JSON.parse(text)), location.origin),
  };
})();
