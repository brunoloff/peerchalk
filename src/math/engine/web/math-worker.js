// Runs the math engine away from the app, so a slow or failing formula cannot stop it.
importScripts("peerchalk_math_worker.js");

const engine = PeerChalkMath();

onmessage = async ({ data }) => {
  const math = await engine;
  const tex = new TextEncoder().encode(data.tex);
  math.HEAPU8.set(tex, math._peerchalk_worker_input(tex.length));
  const result = math._peerchalk_worker_layout(data.display ? 1 : 0, data.rgba, data.tolerance);
  const size = math._peerchalk_worker_output_size();
  const bytes = math.HEAPU8.slice(result, result + size);
  postMessage(bytes.buffer, [bytes.buffer]);
};
