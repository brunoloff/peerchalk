// Starts PeerChalk in the page and shows how far it got. Each phase has a time limit; when one
// runs over, or something fails, the page explains what may be wrong and offers the details for
// a report. The app reports its own phases through window.peerchalkLoader.
(() => {
  const issueUrl = "https://github.com/brunoloff/peerchalk/issues/new";
  const maxIssueDetails = 6000;
  // name, label, what it is doing (for messages), time limit in ms
  const phases = [
    ["check", "Checking this browser", "check this browser", 10000],
    ["script", "Loading the app's code", "load the app's code", 30000],
    ["download", "Downloading the app", "download the app", 30000], // without any progress
    ["compile", "Preparing the app", "prepare the app", 60000],
    ["storage", "Opening your saved data", "open your saved data", 20000],
    ["start", "Starting", "start the app", 30000],
  ];
  // Tests set window.peerchalkLoaderLimits to shorten the limits.
  const limits = Object.assign({}, window.peerchalkLoaderLimits || {});
  const started = performance.now();
  const events = [];
  let current = "check";
  let failed = false;
  let finished = false;
  let timer = 0;

  const $ = (id) => document.getElementById(id);
  const note = (text) => events.push(`${((performance.now() - started) / 1000).toFixed(1)} s  ${text}`);
  const phaseIndex = (name) => phases.findIndex(([n]) => n === name);

  const adBlocker = "An ad or content blocker, or a privacy extension, may be blocking part of " +
    "PeerChalk. Try turning it off for this site, then reload.";
  const stallHints = {
    check: ["Reload the page.", "Make sure your browser is up to date."],
    script: [adBlocker, "Check your internet connection, then reload."],
    download: ["Check your internet connection: the app is about 30 MB.", adBlocker],
    compile: ["Close other tabs or apps to free memory, then reload.",
              "Make sure your browser is up to date."],
    storage: ["Close other PeerChalk tabs, then reload.",
              "Private browsing may not allow saving data: try a normal window."],
    start: ["Turn on hardware acceleration (graphics) in your browser's settings.",
            "Close other PeerChalk tabs, then reload."],
  };

  // progress: how much of the app has arrived, { text, fraction } (fraction 0 when unknown).
  function render(progress) {
    const at = phaseIndex(current);
    $("phases").replaceChildren(...phases.map(([name, label], index) => {
      const item = document.createElement("li");
      item.textContent = label;
      item.className = index < at ? "done" : index > at ? "" : failed ? "failed" : "active";
      if (name === "download" && progress && index === at)
        item.append(...progressParts(progress));
      return item;
    }));
  }

  function progressParts({ text, fraction }) {
    const amount = document.createElement("span");
    amount.className = "amount";
    amount.textContent = text;
    if (!fraction) return [amount];
    const bar = document.createElement("span");
    bar.className = "bar";
    const filled = document.createElement("span");
    filled.style.width = `${Math.min(100, fraction * 100).toFixed(1)}%`;
    bar.append(filled);
    return [amount, bar];
  }

  function watch() {
    clearTimeout(timer);
    const entry = phases[phaseIndex(current)];
    timer = setTimeout(stalled, limits[current] ?? (entry ? entry[3] : 30000));
  }

  // Phases only move forward; the app's storage may open while the code still downloads.
  function phase(name, detail) {
    if (finished || failed || phaseIndex(name) <= phaseIndex(current)) return;
    current = name;
    note(`${name}${detail ? `: ${detail}` : ""}`);
    render();
    watch();
  }

  function stalled() {
    note(`${current} is taking too long`);
    explain(`It is taking unusually long to ${phases[phaseIndex(current)][2]}.`,
            stallHints[current], false);
  }

  function fail(problem, hints) {
    if (finished || failed) return;
    failed = true;
    clearTimeout(timer);
    note(`failed: ${problem}`);
    render();
    explain(`PeerChalk could not start: ${problem}`, hints, true);
  }

  function ready() {
    if (finished) return;
    finished = true;
    clearTimeout(timer);
    note("ready");
    $("loading").hidden = true;
  }

  function graphics() {
    try {
      const canvas = document.createElement("canvas");
      return canvas.getContext("webgl2") ? "WebGL 2" : canvas.getContext("webgl") ? "WebGL 1" : "none";
    } catch (error) {
      return `error: ${error}`;
    }
  }

  function details() {
    return [
      `Build: ${document.querySelector('meta[name="peerchalk-build"]')?.content || "unknown"}`,
      // The address's path only: the part after # can hold a board's secret link.
      `Page: ${location.origin}${location.pathname}`,
      `Browser: ${navigator.userAgent}`,
      `WebAssembly: ${typeof WebAssembly === "object"}, graphics: ${graphics()}, ` +
        `IndexedDB: ${typeof indexedDB === "object"}, Web Locks: ${Boolean(navigator.locks)}`,
      `Online: ${navigator.onLine}, memory: ${navigator.deviceMemory ?? "?"} GB, ` +
        `cores: ${navigator.hardwareConcurrency ?? "?"}, screen: ${screen.width}x${screen.height}`,
      "",
      ...events,
    ].join("\n");
  }

  function issueLink(summary) {
    const body = "**What happened**\n\n(Describe what you saw.)\n\n" +
      "**Details from the loading page**\n\n```\n" + details().slice(-maxIssueDetails) + "\n```\n";
    return `${issueUrl}?title=${encodeURIComponent(summary.slice(0, 120))}` +
      `&body=${encodeURIComponent(body)}`;
  }

  function explain(summary, hints, fatal) {
    $("help").hidden = false;
    $("help-title").textContent = summary;
    $("help-title").className = fatal ? "fatal" : "";
    $("hints").replaceChildren(...(hints || ["Reload the page."]).map((hint) => {
      const item = document.createElement("li");
      item.textContent = hint;
      return item;
    }));
    $("details").textContent = details();
    $("report").href = issueLink(summary);
  }

  // Missing features end the start at once, with advice that fits.
  function checkBrowser() {
    if (typeof WebAssembly !== "object") {
      fail("this browser cannot run WebAssembly", [
        "Update your browser, or try a current Chrome, Edge, Firefox or Safari.",
        "Some security settings turn WebAssembly off; allow it for this site."]);
      return false;
    }
    if (graphics() === "none")
      note("no WebGL: PeerChalk may not be able to draw");
    if (!navigator.locks)
      note("no Web Locks: several PeerChalk tabs may conflict");
    return true;
  }

  function loadScript(source) {
    return new Promise((resolve, reject) => {
      const script = document.createElement("script");
      script.src = source;
      script.onload = resolve;
      script.onerror = () => reject(new Error(`${source} could not be loaded`));
      document.head.append(script);
    });
  }

  // PDFium's own module, which the app reaches for PDFs (src/app/web_pdf.js). It loads beside
  // the app; without it the app still starts, and only PDFs are unavailable.
  function loadPdfium() {
    const limitMs = 60000;
    return new Promise((resolve) => {
      const give = (text) => {
        if (text) note(text);
        resolve();
      };
      setTimeout(() => give(window.peerchalkPdfium ? "" : "the PDF engine took too long"), limitMs);
      window.Module = { // pdfium.js is a plain script: it takes its settings from this global
        locateFile: (path) => path,
        print: () => {},
        printErr: (text) => note(`pdf: ${text}`),
        onRuntimeInitialized() {
          window.peerchalkPdfium = window.Module;
          give("PDF engine ready");
        },
      };
      loadScript("pdfium.js").catch(() => give("the PDF engine could not be loaded"));
    });
  }

  // Downloads the app with progress, so a stopped download is noticed.
  async function download(url) {
    const response = await fetch(url);
    if (!response.ok)
      throw new Error(`downloading ${url} failed with HTTP ${response.status}`);
    if (!response.body)
      return response.arrayBuffer();
    const reader = response.body.getReader();
    const chunks = [];
    let received = 0;
    // A compressed response's length counts compressed bytes, so it gives no fraction.
    const total = response.headers.get("Content-Encoding")
      ? 0 : Number(response.headers.get("Content-Length")) || 0;
    const megabytes = (bytes) => (bytes / 1048576).toFixed(1);
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      chunks.push(value);
      received += value.length;
      render({ text: total ? `${megabytes(received)} / ${megabytes(total)} MB` : `${megabytes(received)} MB`,
               fraction: total ? received / total : 0 });
      watch();
    }
    note(`downloaded ${(received / 1048576).toFixed(1)} MB`);
    return new Blob(chunks).arrayBuffer();
  }

  function instantiate(imports, done) {
    phase("download");
    download("peerchalk.wasm")
      .catch((error) => {
        fail(`the app could not be downloaded (${error.message || error})`,
             [adBlocker, "Check your internet connection, then reload."]);
        throw error;
      })
      .then((bytes) => {
        phase("compile");
        return WebAssembly.instantiate(bytes, imports);
      })
      .then(({ instance, module }) => {
        phase("storage");
        done(instance, module);
      })
      .catch((error) => {
        if (!failed)
          fail(`the app could not be prepared (${error.message || error})`,
               ["Close other tabs or apps to free memory, then reload.",
                "Make sure your browser is up to date."]);
      });
    return {};
  }

  async function start() {
    const screen = $("screen");
    note("check");
    render();
    watch();
    if (!checkBrowser()) return;
    phase("script");
    const pdfium = loadPdfium();
    try {
      await loadScript("peerchalk.js");
    } catch (error) {
      fail("the app's code could not be loaded", [adBlocker, "Check your internet connection, then reload."]);
      return;
    }
    screen.style.display = "block";
    try {
      const app = await window.peerchalk_entry({
        qtContainerElements: [screen],
        noInitialRun: true,
        instantiateWasm: instantiate,
        printErr: (text) => note(`app: ${text}`),
        onAbort: (reason) => fail(`the app stopped (${reason})`, ["Reload the page."]),
        onRuntimeInitialized: () => note("runtime ready"),
      });
      await pdfium;
      phase("start");
      app.callMain([]);
    } catch (error) {
      if (error !== "unwind") // what the app's event loop throws when it starts normally
        fail(`the app stopped (${error.message || error})`, ["Reload the page."]);
    }
  }

  // The computer system in the browser's user agent, as AccountManager's
  // systemFromUserAgent names it: "" for phones, tablets and others.
  function visitorSystem() {
    const agent = navigator.userAgent;
    if (/Android|iPhone|iPad|iPod|Mobile/.test(agent)) return "";
    if (agent.includes("Windows")) return "Windows";
    if (agent.includes("Macintosh")) return "macOS";
    if (agent.includes("Linux") && !agent.includes("CrOS")) return "Linux";
    return "";
  }

  // Names the visitor's system on the download button, as the account screen does.
  function offerApp() {
    const system = visitorSystem();
    if (!system) return;
    $("download").textContent = `Download for ${system}`;
    const others = document.createElement("a");
    others.href = $("download").href;
    others.textContent = ["Linux", "Windows", "macOS"].filter((name) => name !== system).join(" and ");
    $("systems").replaceChildren("The app for your computer starts faster than this browser version " +
      "and keeps boards outside the browser's storage. Also for ", others, ".");
  }

  addEventListener("error", (event) => note(`page error: ${event.message}`));
  addEventListener("unhandledrejection", (event) => note(`unhandled: ${event.reason}`));
  $("copy").addEventListener("click", async () => {
    try {
      await navigator.clipboard.writeText(details());
      $("copy").textContent = "Copied";
    } catch (error) {
      note(`copying failed: ${error}`);
      $("details-box").open = true;
    }
  });
  $("reload").addEventListener("click", () => location.reload());

  // The joke on the logo shows on the first visit only; the browser remembers the visit.
  function markFirstVisit() {
    try {
      if (localStorage.getItem("peerchalk-visited")) return;
      localStorage.setItem("peerchalk-visited", "1");
      document.body.classList.add("first-visit");
    } catch (error) {
      note(`no local storage: ${error}`);
    }
  }

  window.peerchalkLoader = { phase, ready, note, details };
  markFirstVisit();
  offerApp();
  start();
})();
