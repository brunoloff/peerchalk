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

namespace {

int stepUp(double pixels, int highest) {
    if (!(pixels > minHeight))
        return minHeight;
    const double steps = std::ceil(std::log(pixels) / std::log(std::sqrt(2.0)) - 1e-9);
    return std::clamp(static_cast<int>(std::lround(std::pow(std::sqrt(2.0), steps))), minHeight,
                      highest);
}

} // namespace

int PdfRenderer::bucketFor(double pixels) { return stepUp(pixels, maxHeight); }

int PdfRenderer::tiledHeightFor(double pixels) { return stepUp(pixels, 32768); }

std::vector<PdfRenderer::Tile> PdfRenderer::tilesIn(const Bounds &page, const Bounds &visible,
                                                    int height) {
    constexpr int size = tileSize;
    const double perUnit = height / double(page.height()); // pixels per board unit
    const int width = static_cast<int>(std::lround(height * page.width() / page.height()));
    const int columns = (width + size - 1) / size;
    const int rows = (height + size - 1) / size;
    const auto cell = [&](float units, int count) {
        return std::clamp(static_cast<int>(std::floor(units * perUnit / size)), 0, count - 1);
    };
    std::vector<Tile> tiles;
    if (visible.right < page.left || visible.left > page.right || visible.bottom < page.top ||
        visible.top > page.bottom)
        return tiles;
    const float step = static_cast<float>(size / perUnit);
    for (int row = cell(visible.top - page.top, rows); row <= cell(visible.bottom - page.top, rows);
         ++row) {
        for (int col = cell(visible.left - page.left, columns);
             col <= cell(visible.right - page.left, columns); ++col) {
            const float left = page.left + col * step;
            const float top = page.top + row * step;
            tiles.push_back({row * columns + col,
                             {left, top, std::min(page.right, left + step),
                              std::min(page.bottom, top + step)}});
        }
    }
    return tiles;
}

void PdfRenderer::want(std::vector<Request> requests) {
    {
        const std::lock_guard lock(lock_);
        std::erase_if(requests, [this](const Request &request) {
            return failed_.contains(request.source.blob) ||
                   images_.contains(
                       {request.source.blob, request.source.page, request.height, request.tile});
        });
        queue_ = std::move(requests);
    }
    wake_.notify_all();
}

QImage PdfRenderer::best(const PdfSource &source, int height) const {
    const std::lock_guard lock(lock_);
    const auto first = images_.lower_bound({source.blob, source.page, 0, -1});
    Entry *found = nullptr;
    int foundHeight = 0;
    for (auto it = first; it != images_.end() && std::get<0>(it->first) == source.blob &&
                          std::get<1>(it->first) == source.page;
         ++it) {
        if (std::get<3>(it->first) != -1)
            continue; // a tile, not the whole page
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

QImage PdfRenderer::tile(const PdfSource &source, int height, int tile) const {
    const std::lock_guard lock(lock_);
    const auto found = images_.find({source.blob, source.page, height, tile});
    if (found == images_.end())
        return {};
    found->second.used = ++clock_;
    return found->second.image;
}

bool PdfRenderer::failed(const std::string &blob) const {
    const std::lock_guard lock(lock_);
    return failed_.contains(blob);
}

void PdfRenderer::forgetFailures() {
    {
        const std::lock_guard lock(lock_);
        failed_.clear();
    }
    emit retryWanted();
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
        static_cast<int>(std::lround(request.height * size.width / size.height)), 1, 65536);
    PdfImage rendered;
    if (request.tile < 0) {
        rendered = pdf->render(source.page, std::min(width, 16384), request.height);
    } else {
        const int columns = (width + tileSize - 1) / tileSize;
        const int x = request.tile % columns * tileSize;
        const int y = request.tile / columns * tileSize;
        if (x >= width || y >= request.height)
            return; // a tile beyond the page
        rendered =
            pdf->renderTile(source.page, width, request.height, x, y, std::min(tileSize, width - x),
                            std::min(tileSize, request.height - y));
    }
    QImage image(rendered.width, rendered.height, QImage::Format_RGB32);
    for (int row = 0; row < rendered.height; ++row)
        std::memcpy(image.scanLine(row),
                    rendered.pixels.data() + std::size_t(row) * rendered.width * 4,
                    std::size_t(rendered.width) * 4);
    store({source.blob, source.page, request.height, request.tile}, std::move(image));
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
