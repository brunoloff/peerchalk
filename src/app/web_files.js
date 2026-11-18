// Linked into the browser build with --pre-js: saving and choosing files for web_files.cpp.
function peerchalkDownloadFile(name, nameSize, data, size) {
  const fileName = new TextDecoder().decode(HEAPU8.slice(name, name + nameSize));
  const url = URL.createObjectURL(new Blob([HEAPU8.slice(data, data + size)]));
  const link = document.createElement("a");
  link.href = url;
  link.download = fileName;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
}

function peerchalkPickFile() {
  const input = document.createElement("input");
  input.type = "file";
  input.accept = ".peerchalk";
  input.onchange = async () => {
    if (input.files.length === 0) return;
    const bytes = new Uint8Array(await input.files[0].arrayBuffer());
    const target = _peerchalk_file_buffer(bytes.length);
    HEAPU8.set(bytes, target);
    _peerchalk_file_ready();
  };
  input.click();
}

// The direct link between this browser's PeerChalk tabs (see instance_link_web.cpp).
let peerchalkLinkChannel = null;

function peerchalkLinkOpen() {
  peerchalkLinkChannel = new BroadcastChannel("peerchalk-instances");
  peerchalkLinkChannel.onmessage = (event) => {
    const bytes = new Uint8Array(event.data);
    HEAPU8.set(bytes, _peerchalk_link_buffer(bytes.length));
    _peerchalk_link_ready();
  };
}

function peerchalkLinkPost(data, size) {
  peerchalkLinkChannel.postMessage(HEAPU8.slice(data, data + size).buffer);
}
