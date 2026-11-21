// Runs src/app/web_pdf.js against PDFium's real WebAssembly module, the way the browser build
// uses them, with a plain array standing in for the app's memory:
//   node tests/web/pdf_bridge_test.mjs build/prefix-web/pdfium
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { createRequire } from "node:module";
import path from "node:path";
import vm from "node:vm";

const folder = path.resolve(process.argv[2] ?? "build/prefix-web/pdfium");
const root = path.resolve(path.dirname(new URL(import.meta.url).pathname), "../..");

// PDFium's module: a plain script that fills in the global Module.
const pdfium = await new Promise((resolve) => {
  globalThis.Module = { onRuntimeInitialized: () => resolve(globalThis.Module) };
  globalThis.require = createRequire(path.join(folder, "pdfium.js"));
  globalThis.__dirname = folder;
  vm.runInThisContext(readFileSync(path.join(folder, "pdfium.js"), "utf8"));
});
// The app's side, in a scope of its own as in the page: its memory, and the bridge's script
// as the app links it.
const HEAPU8 = new Uint8Array(1 << 22);
const app = vm.createContext({ HEAPU8, TextEncoder, TextDecoder, Map, Math, peerchalkPdfium: pdfium });
vm.runInContext(readFileSync(path.join(root, "src/app/web_pdf.js"), "utf8") +
                "\nglobalThis.peerchalkPdf = peerchalkPdf;", app);
const bridge = app.peerchalkPdf;

// A two-page PDF: an A4 page with a black square at its bottom left, then a Letter page.
function samplePdf() {
  const stream = "0 0 0 rg 0 0 100 100 re f";
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595.28 841.89] /Contents 5 0 R >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    "<< /Title (Lemma notes) >>",
  ];
  let pdf = "%PDF-1.4\n";
  const offsets = objects.map((object, i) => {
    const at = pdf.length;
    pdf += `${i + 1} 0 obj\n${object}\nendobj\n`;
    return at;
  });
  const xref = pdf.length;
  pdf += `xref\n0 ${objects.length + 1}\n0000000000 65535 f \n`;
  for (const offset of offsets) pdf += `${String(offset).padStart(10, "0")} 00000 n \n`;
  pdf += `trailer\n<< /Size ${objects.length + 1} /Root 1 0 R /Info 6 0 R >>\n` +
         `startxref\n${xref}\n%%EOF\n`;
  return new TextEncoder().encode(pdf);
}

assert.equal(bridge.present(), 1);
bridge.call("FPDF_InitLibrary");
const bytes = samplePdf();
HEAPU8.set(bytes, 1024);
const document = bridge.load(1024, bytes.length);
assert.ok(document, "the PDF opens");
assert.equal(bridge.call("FPDF_GetPageCount", document), 2);

const sizeAt = 64;
assert.equal(bridge.pageSize(document, 1, sizeAt), 1);
const size = new Float32Array(HEAPU8.buffer, sizeAt, 2);
assert.deepEqual([...size], [612, 792]);

const titleAt = 128;
const titleBytes = bridge.metaText(document, "Title", titleAt, 64);
const title = new TextDecoder("utf-16le").decode(HEAPU8.slice(titleAt, titleAt + titleBytes - 2));
assert.equal(title, "Lemma notes");

// Page one at 100 × 141 pixels: the square is black, the rest white.
const page = bridge.call("FPDF_LoadPage", document, 0);
const bitmap = bridge.call("FPDFBitmap_Create", 100, 141, 0);
bridge.call("FPDFBitmap_FillRect", bitmap, 0, 0, 100, 141, 0xffffffff);
bridge.call("FPDF_RenderPageBitmap", bitmap, page, 0, 0, 100, 141, 0, 0);
const stride = bridge.call("FPDFBitmap_GetStride", bitmap);
const pixelsAt = 65536;
bridge.bitmapCopy(bitmap, pixelsAt, stride * 141);
const pixel = (x, y) => HEAPU8[pixelsAt + y * stride + x * 4];
assert.equal(pixel(3, 138), 0);
assert.equal(pixel(80, 10), 255);

// Saving the document hands the whole file back.
const saved = bridge.save(document, 2);
assert.ok(saved > 0);
const savedAt = 200000;
bridge.takeSaved(savedAt);
assert.equal(new TextDecoder().decode(HEAPU8.slice(savedAt, savedAt + 5)), "%PDF-");

bridge.call("FPDFBitmap_Destroy", bitmap);
bridge.call("FPDF_ClosePage", page);
bridge.close(document);
assert.equal(bridge.documents.size, 0);
console.log("web_pdf.js works with PDFium's module");
