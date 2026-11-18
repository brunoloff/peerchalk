// Linked into the browser build with --pre-js: runs formulas for math_engine_web.cpp in a
// Web Worker, one at a time. A worker that takes too long or fails is replaced.
const peerchalkMathTimeoutMs = 3000;
const peerchalkMathHeaderBytes = 40;
const peerchalkMath = { worker: null, queue: [], active: null, timer: 0 };

function peerchalkMathDeliver(id, bytes) {
  const target = _peerchalk_math_result_buffer(bytes.length);
  HEAPU8.set(bytes, target);
  _peerchalk_math_result_ready(id);
}

function peerchalkMathError(message) {
  const text = new TextEncoder().encode(message);
  const bytes = new Uint8Array(peerchalkMathHeaderBytes + text.length);
  new DataView(bytes.buffer).setUint32(12, text.length, true);
  bytes.set(text, peerchalkMathHeaderBytes);
  return bytes;
}

function peerchalkMathFinish(bytes) {
  clearTimeout(peerchalkMath.timer);
  const id = peerchalkMath.active.id;
  peerchalkMath.active = null;
  peerchalkMathDeliver(id, bytes);
  peerchalkMathNext();
}

function peerchalkMathAbandon(message) {
  console.error("PeerChalk math worker:", message);
  peerchalkMath.worker?.terminate();
  peerchalkMath.worker = null;
  if (peerchalkMath.active) peerchalkMathFinish(peerchalkMathError(message));
}

function peerchalkMathStartWorker() {
  const worker = new Worker(new URL("math-worker.js", document.baseURI));
  worker.onmessage = (event) => peerchalkMathFinish(new Uint8Array(event.data));
  worker.onerror = (event) => peerchalkMathAbandon(`the math engine failed: ${event.message}`);
  return worker;
}

function peerchalkMathNext() {
  if (peerchalkMath.active || peerchalkMath.queue.length === 0) return;
  peerchalkMath.active = peerchalkMath.queue.shift();
  peerchalkMath.worker ??= peerchalkMathStartWorker();
  peerchalkMath.timer = setTimeout(
    () => peerchalkMathAbandon("the formula took too long to lay out"), peerchalkMathTimeoutMs);
  peerchalkMath.worker.postMessage(peerchalkMath.active);
}

function peerchalkMathRequest(id, tex, size, display, rgba, tolerance) {
  const text = new TextDecoder().decode(HEAPU8.slice(tex, tex + size));
  peerchalkMath.queue.push({ id, tex: text, display, rgba, tolerance });
  peerchalkMathNext();
}
