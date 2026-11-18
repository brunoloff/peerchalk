// Linked into the browser build with --pre-js. Browsers give the app an in-memory file
// system, so this keeps its data in IndexedDB. Any number of tabs can run at once: each
// claims a numbered slot with a database of its own (guarded by a Web Lock), loads the other
// slots to merge them when the app starts, and removes those whose tabs have closed.
(() => {
  const baseRoot = "/peerchalk";
  const lockPrefix = "peerchalk-instance-";
  const saveIntervalMs = 1000;
  const runningCheckMs = 2000;
  let index = 0;
  let others = [];
  let anotherIsRunning = false;
  const heldLocks = new Map(); // slots of ended tabs, kept until they have been merged
  let saving = false;
  let saveAgain = false;
  let afterSave = null;

  const rootFor = (slot) => (slot === 0 ? baseRoot : `${baseRoot}-${slot}`);

  function slotOfDatabase(name) {
    const match = /^\/peerchalk(?:-(\d+))?$/.exec(name);
    return match ? Number(match[1] || 0) : null;
  }

  // Resolves to a function releasing the lock, or to null if another tab holds it.
  function claim(slot) {
    return new Promise((resolve, reject) => {
      navigator.locks.request(`${lockPrefix}${slot}`, { ifAvailable: true }, (lock) => {
        if (!lock) {
          resolve(null);
          return undefined;
        }
        return new Promise((release) => resolve(release));
      }).catch(reject);
    });
  }

  async function findOthers() {
    if (!indexedDB.databases) return;
    for (const { name } of await indexedDB.databases()) {
      const slot = slotOfDatabase(name);
      if (slot === null || slot === index) continue;
      const release = await claim(slot);
      if (release) heldLocks.set(slot, release);
      others.push({ index: slot, root: rootFor(slot), running: !release });
    }
    anotherIsRunning = others.some((other) => other.running);
  }

  async function claimSlot() {
    if (!navigator.locks) {
      console.warn("This browser cannot run several PeerChalk tabs safely.");
      return;
    }
    while (!(await claim(index))) index += 1;
    await findOthers();
  }

  async function refreshRunning() {
    const { held } = await navigator.locks.query();
    const ours = `${lockPrefix}${index}`;
    const merging = new Set([...heldLocks.keys()].map((slot) => `${lockPrefix}${slot}`));
    anotherIsRunning = held.some(({ name }) =>
      name.startsWith(lockPrefix) && name !== ours && !merging.has(name));
  }

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
      } else if (afterSave) {
        const done = afterSave;
        afterSave = null;
        done();
      }
    });
  }

  function removeDatabase(root) {
    if (IDBFS.dbs[root]) {
      IDBFS.dbs[root].close();
      delete IDBFS.dbs[root];
    }
    indexedDB.deleteDatabase(root);
  }

  function startSaving() {
    setInterval(save, saveIntervalMs);
    addEventListener("pagehide", save);
    document.addEventListener("visibilitychange", () => {
      if (document.visibilityState === "hidden") save();
    });
    if (navigator.locks) setInterval(() => refreshRunning().catch(console.error), runningCheckMs);
  }

  // The merged data is saved here first; only then are ended tabs' databases removed.
  function finishMerge(endedRoots) {
    for (const other of others) FS.unmount(other.root);
    afterSave = () => {
      endedRoots.forEach(removeDatabase);
      for (const release of heldLocks.values()) release();
      heldLocks.clear();
    };
    startSaving();
    save();
  }

  window.peerchalkStorage = {
    state: () => ({ index, root: rootFor(index), others }),
    finishMerge,
    othersRunning: () => anotherIsRunning,
  };

  function restored(error) {
    if (error) {
      console.error("PeerChalk could not open browser storage:", error);
      alert("PeerChalk cannot use this browser's storage, so nothing will be kept after this page closes.");
    } else if (navigator.storage && navigator.storage.persist) {
      // Asks the browser not to evict this data when space runs low.
      navigator.storage.persist().catch((failure) => {
        console.warn("PeerChalk could not request persistent storage:", failure);
      });
    }
    removeRunDependency("peerchalk-storage");
  }

  function mountStorage() {
    addRunDependency("peerchalk-storage");
    claimSlot().then(() => {
      for (const root of [rootFor(index), ...others.map((other) => other.root)]) {
        FS.mkdir(root);
        FS.mount(IDBFS, {}, root);
      }
      FS.syncfs(true, restored);
    }).catch(restored);
  }

  Module.preRun = [].concat(Module.preRun || [], mountStorage);
})();
