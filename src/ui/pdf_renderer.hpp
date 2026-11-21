#pragma once

#include "board/page_layout.hpp"

#include <QImage>
#include <QObject>

#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace peerchalk {

class BlobStore;
class PdfDocument;

// Renders pages of PDFs on a worker thread into images cached by size, within a memory
// budget. The view says what it wants as a whole (want()), most urgent first, so pages that
// left it are not rendered; it draws the nearest size it has meanwhile (best()).
class PdfRenderer : public QObject {
    Q_OBJECT

  public:
    struct Request {
        PdfSource source;
        int height; // pixels, from bucketFor()
    };

    PdfRenderer(const BlobStore &blobs, std::size_t budgetBytes, QObject *parent = nullptr);
    ~PdfRenderer() override;
    PdfRenderer(const PdfRenderer &) = delete;
    PdfRenderer &operator=(const PdfRenderer &) = delete;

    // The height to render a page shown `pixels` tall at: the next power of √2, between 64
    // and 4096, so that zooming reuses images.
    [[nodiscard]] static int bucketFor(double pixels);

    void want(std::vector<Request> requests);
    // The cached image of the page with the height nearest to height, preferring larger;
    // null when there is none yet. Any thread.
    [[nodiscard]] QImage best(const PdfSource &source, int height) const;
    // Pages whose PDF is missing or unreadable here, by blob; a blob that arrives later
    // is tried again after forgetFailures().
    [[nodiscard]] bool failed(const std::string &blob) const;
    void forgetFailures();

  signals:
    // An image was added; views should draw again.
    void rendered();

  private:
    using Key = std::tuple<std::string, int, int>; // blob, page, height
    struct Entry {
        QImage image;
        std::uint64_t used;
    };

    void run();
    void renderOne(const Request &request);
    std::shared_ptr<PdfDocument> document(const std::string &blob);
    void store(const Key &key, QImage image);

    const BlobStore &blobs_;
    std::size_t budget_;
    mutable std::mutex lock_;
    std::condition_variable wake_;
    std::vector<Request> queue_;
    bool stopping_{false};
    mutable std::map<Key, Entry> images_; // best() records use
    std::size_t bytes_{0};
    mutable std::uint64_t clock_{0};
    std::set<std::string> failed_;
    std::map<std::string, std::shared_ptr<PdfDocument>> documents_; // worker thread only
    std::thread worker_;
};

} // namespace peerchalk
