// Builds tests/web/pdf_web_check.cpp for WebAssembly against the browser build's libraries and
// runs it under Node beside PDFium's module, as the page loads them:
//   source .emsdk/emsdk_env.sh && node tests/web/run_pdf_web_check.mjs
import { execFileSync } from "node:child_process";
import { mkdirSync, readFileSync } from "node:fs";
import { createRequire } from "node:module";
import path from "node:path";
import vm from "node:vm";

const root = path.resolve(path.dirname(new URL(import.meta.url).pathname), "../..");
const out = path.join(root, "build/web/pdf-web-check");
const pdfium = path.join(root, "build/prefix-web/pdfium");
mkdirSync(out, { recursive: true });
execFileSync("em++", [
  "-std=c++20", "-O1", `-I${root}/src`, `-I${root}/build/prefix-web/include/pdfium`,
  path.join(root, "tests/web/pdf_web_check.cpp"),
  path.join(root, "build/web/src/libpeerchalk_pdf.a"),
  path.join(root, "build/web/src/libpeerchalk_board.a"),
  "--pre-js", path.join(root, "src/app/web_pdf.js"),
  "-sMODULARIZE=1", "-sEXPORT_NAME=createCheck", "-sENVIRONMENT=node", "-sEXIT_RUNTIME=1",
  "-sALLOW_MEMORY_GROWTH=1", "-o", path.join(out, "check.js"),
], { stdio: "inherit" });

// PDFium's module, a plain script, as the page loads it.
globalThis.peerchalkPdfium = await new Promise((resolve) => {
  globalThis.Module = { onRuntimeInitialized: () => resolve(globalThis.Module) };
  globalThis.require = createRequire(path.join(pdfium, "pdfium.js"));
  globalThis.__dirname = pdfium;
  vm.runInThisContext(readFileSync(path.join(pdfium, "pdfium.js"), "utf8"));
});
// PDFium's script goes on using the global Module, as it does in the page.

const createCheck = createRequire(import.meta.url)(path.join(out, "check.js"));
let status = 0;
try {
  await createCheck({ onExit: (code) => { status = code; } });
} catch (error) {
  // PDFium's minified source makes Node's own report unreadable.
  console.error(`the check stopped: ${error?.message ?? error}`);
  console.error(String(error?.stack ?? "").split("\n").map((line) => line.slice(0, 200)).join("\n"));
  status = 1;
}
process.exit(status);
