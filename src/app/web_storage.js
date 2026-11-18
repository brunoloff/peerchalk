// Linked into the browser build with --pre-js. Browsers give the app an in-memory file
// system, so this keeps PeerChalk's data folder in IndexedDB: it is restored before
// main() runs and saved shortly after every change.
(() => {
  const directory = "/peerchalk";
  const saveIntervalMs = 1000;
  let saving = false;
  let saveAgain = false;

  function save() {
    if (saving) {
      saveAgain = true;
      return;
    }
    saving = true;
    FS.syncfs(false, (error) => {
      saving = false;
      if (error) console.error("PeerChalk could not save to browser storage:", error);
      if (saveAgain) {
        saveAgain = false;
        save();
      }
    });
  }

  function restored(error) {
    if (error) {
      console.error("PeerChalk could not open browser storage:", error);
      alert("PeerChalk cannot use this browser's storage, so nothing will be kept after this page closes.");
    } else {
      // Asks the browser not to evict this data when space runs low.
      if (navigator.storage && navigator.storage.persist) {
        navigator.storage.persist().catch((failure) => {
          console.warn("PeerChalk could not request persistent storage:", failure);
        });
      }
      setInterval(save, saveIntervalMs);
      addEventListener("pagehide", save);
      document.addEventListener("visibilitychange", () => {
        if (document.visibilityState === "hidden") save();
      });
    }
    removeRunDependency("peerchalk-storage");
  }

  function mountStorage() {
    FS.mkdir(directory);
    FS.mount(IDBFS, {}, directory);
    addRunDependency("peerchalk-storage");
    FS.syncfs(true, restored);
  }

  Module.preRun = [].concat(Module.preRun || [], mountStorage);
})();
