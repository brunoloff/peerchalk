#pragma once

// What every use of PDFium shares: it is not thread-safe, so callers hold one lock.
#include <mutex>

namespace peerchalk::pdf_engine {

[[nodiscard]] std::mutex &lock();
// Starts PDFium the first time; call it holding lock().
void initialise();

} // namespace peerchalk::pdf_engine
