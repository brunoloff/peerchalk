// Network: fetching the PDFs that shared page boards show, from a peer that has them. A blob
// travels in pieces, one request at a time, from one peer; it is stored only when its
// SHA-256 matches, and given only to peers that share a board showing it.
#include "sync/network.hpp"

#include "board/board_pages.hpp"
#include "library/blob_store.hpp"
#include "ui/library.hpp"

#include <QtLogging>

namespace peerchalk::sync {

namespace {

constexpr qsizetype blobPieceBytes = qsizetype{256} * 1024;

QByteArray rawHash(const std::string &hex) { return QByteArray::fromHex(QByteArray(hex.c_str())); }

std::string hexHash(const QByteArray &raw) { return raw.toHex().toStdString(); }

} // namespace

void Network::requestMissingBlobs(const PeerId &id, Peer &peer, const QUuid &board) {
    if (!syncable(peer, board))
        return;
    BlobStore &blobs = library_.blobs();
    bool asked = false;
    for (const std::string &hash : library_.document(board).board().pages().pdfBlobs()) {
        if (!BlobStore::isHash(QString::fromStdString(hash)) ||
            blobs.has(QString::fromStdString(hash)) || downloads_.contains(hash))
            continue;
        downloads_.emplace(hash, Download{id, {}});
        queue(peer, BlobWant{rawHash(hash), 0});
        asked = true;
    }
    if (asked)
        flush(id);
}

bool Network::blobSharedWith(const Peer &peer, const std::string &hash) const {
    for (const QUuid &board : syncableBoards(peer)) {
        if (library_.document(board).board().pages().pdfBlobs().contains(hash))
            return true;
    }
    return false;
}

void Network::handleBlobWant(Peer &peer, const BlobWant &want) {
    const std::string hash = hexHash(want.hash);
    if (!blobSharedWith(peer, hash))
        return; // only to those who may see a board that shows it
    const std::shared_ptr<const Bytes> data = library_.blobs().get(QString::fromStdString(hash));
    if (!data || want.offset >= data->size() || data->size() > maxBlobBytes)
        return;
    const auto size = std::min<quint64>(blobPieceBytes, data->size() - want.offset);
    const auto *begin = reinterpret_cast<const char *>(data->data()) + want.offset;
    queue(peer, BlobChunk{want.hash, want.offset, data->size(),
                          QByteArray(begin, static_cast<qsizetype>(size))});
}

void Network::handleBlobChunk(const PeerId &id, Peer &peer, const BlobChunk &chunk) {
    const std::string hash = hexHash(chunk.hash);
    const auto download = downloads_.find(hash);
    if (download == downloads_.end() || download->second.from != id ||
        chunk.offset != static_cast<quint64>(download->second.data.size()))
        return; // not asked for, or not the piece asked for
    QByteArray &data = download->second.data;
    data += chunk.data;
    if (static_cast<quint64>(data.size()) < chunk.total) {
        queue(peer, BlobWant{chunk.hash, static_cast<quint64>(data.size())});
        flush(id);
        return;
    }
    const QByteArray complete = std::move(data);
    downloads_.erase(download);
    if (BlobStore::hashOf(complete).toStdString() != hash) {
        qWarning("PeerChalk: a peer sent a PDF that does not match its hash");
        return;
    }
    try {
        library_.blobs().put(complete);
        library_.blobArrived();
    } catch (const std::exception &error) {
        qWarning("PeerChalk: could not keep a PDF from a peer: %s", error.what());
    }
}

void Network::dropDownloadsFrom(const PeerId &id) {
    std::erase_if(downloads_, [&](const auto &entry) { return entry.second.from == id; });
}

} // namespace peerchalk::sync
