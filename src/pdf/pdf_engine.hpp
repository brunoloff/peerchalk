#pragma once

// What every use of PDFium shares: it is not thread-safe, so callers hold one lock.
#include <mutex>

namespace peerchalk::pdf_engine {

[[nodiscard]] std::mutex &lock();
// Starts PDFium the first time; call it holding lock().
void initialise();
// Whether PDFium is there: always on desktops, once its module has loaded in browsers.
[[nodiscard]] bool present();

} // namespace peerchalk::pdf_engine
