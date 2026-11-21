#include "ui/pdf_renderer.hpp"

#include "library/blob_store.hpp"
#include "pdf/pdf_document.hpp"

#include <QtLogging>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace peerchalk {
namespace {

constexpr int minHeight = 64;
constexpr int maxHeight = 4096;
constexpr std::size_t openDocuments = 4;

} // namespace

PdfRenderer::PdfRenderer(const BlobStore &blobs, std::size_t budgetBytes, QObject *parent)
    : QObject(parent), blobs_(blobs), budget_(budgetBytes), worker_([this] { run(); }) {}

PdfRenderer::~PdfRenderer() {
    {
        const std::lock_guard lock(lock_);
        stopping_ = true;
    }
    wake_.notify_all();
    worker_.join();
}

int PdfRenderer::bucketFor(double pixels) {
    if (!(pixels > minHeight))
        return minHeight;
    const double steps = std::ceil(std::log(pixels) / std::log(std::sqrt(2.0)) - 1e-9);
    return std::clamp(static_cast<int>(std::lround(std::pow(std::sqrt(2.0), steps))), minHeight,
                      maxHeight);
}

void PdfRenderer::want(std::vector<Request> requests) {
    {
        const std::lock_guard lock(lock_);
        std::erase_if(requests, [this](const Request &request) {
            return failed_.contains(request.source.blob) ||
                   images_.contains({request.source.blob, request.source.page, request.height});
        });
        queue_ = std::move(requests);
    }
    wake_.notify_all();
}

QImage PdfRenderer::best(const PdfSource &source, int height) const {
    const std::lock_guard lock(lock_);
    const auto first = images_.lower_bound({source.blob, source.page, 0});
    Entry *found = nullptr;
    int foundHeight = 0;
    for (auto it = first; it != images_.end() && std::get<0>(it->first) == source.blob &&
                          std::get<1>(it->first) == source.page;
         ++it) {
        const int h = std::get<2>(it->first);
        // The smallest at least as tall, else the tallest.
        const bool better = found == nullptr || (foundHeight < height && h > foundHeight) ||
                            (h >= height && h < foundHeight);
        if (better) {
            found = &it->second;
            foundHeight = h;
        }
    }
    if (found == nullptr)
        return {};
    found->used = ++clock_;
    return found->image;
}

bool PdfRenderer::failed(const std::string &blob) const {
    const std::lock_guard lock(lock_);
    return failed_.contains(blob);
}

void PdfRenderer::forgetFailures() {
    const std::lock_guard lock(lock_);
    failed_.clear();
}

void PdfRenderer::run() {
    std::unique_lock lock(lock_);
    while (true) {
        wake_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
        if (stopping_)
            return;
        const Request next = queue_.front();
        queue_.erase(queue_.begin());
        lock.unlock();
        renderOne(next);
        lock.lock();
    }
}

std::shared_ptr<PdfDocument> PdfRenderer::document(const std::string &blob) {
    if (const auto open = documents_.find(blob); open != documents_.end())
        return open->second;
    const std::shared_ptr<const Bytes> data = blobs_.get(QString::fromStdString(blob));
    PdfDocument::Opened opened = PdfDocument::open(data);
    if (!opened.document)
        return nullptr;
    if (documents_.size() >= openDocuments)
        documents_.erase(documents_.begin());
    return documents_[blob] = std::move(opened.document);
}

void PdfRenderer::renderOne(const Request &request) {
    const PdfSource &source = request.source;
    const std::shared_ptr<PdfDocument> pdf = document(source.blob);
    if (!pdf || source.page >= pdf->pageCount()) {
        const std::lock_guard lock(lock_);
        failed_.insert(source.blob);
        std::erase_if(queue_, [&](const Request &r) { return r.source.blob == source.blob; });
        return;
    }
    const PaperSize size = pdf->pageSize(source.page);
    const int width = std::clamp(
        static_cast<int>(std::lround(request.height * size.width / size.height)), 1, 16384);
    const PdfImage rendered = pdf->render(source.page, width, request.height);
    QImage image(rendered.width, rendered.height, QImage::Format_RGB32);
    for (int row = 0; row < rendered.height; ++row)
        std::memcpy(image.scanLine(row), rendered.pixels.data() + std::size_t(row) * width * 4,
                    std::size_t(width) * 4);
    store({source.blob, source.page, request.height}, std::move(image));
    QMetaObject::invokeMethod(this, &PdfRenderer::rendered, Qt::QueuedConnection);
}

void PdfRenderer::store(const Key &key, QImage image) {
    const std::lock_guard lock(lock_);
    bytes_ += static_cast<std::size_t>(image.sizeInBytes());
    images_[key] = {std::move(image), ++clock_};
    while (bytes_ > budget_ && images_.size() > 1) {
        const auto oldest = std::ranges::min_element(
            images_, {}, [](const auto &entry) { return entry.second.used; });
        bytes_ -= static_cast<std::size_t>(oldest->second.image.sizeInBytes());
        images_.erase(oldest);
    }
}

} // namespace peerchalk
