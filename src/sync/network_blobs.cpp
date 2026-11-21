// Network: fetching the PDFs that shared page boards show, from a peer that has them. A blob
// travels in pieces, one request at a time, from one peer; it is stored only when its
// SHA-256 matches, and given only to peers that share a board showing it.
#include "sync/network.hpp"

#include "sync/connection_log.hpp"

#include "board/board_pages.hpp"
#include "library/blob_store.hpp"
#include "ui/library.hpp"

#include <QtLogging>

#include <algorithm>

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
        downloads_[hash] = Download{id, {}, {}};
        downloads_[hash].quiet.start();
        downloadTimer_.start();
        library_.setBlobProgress(QString::fromStdString(hash), 0, 0, peer.user->name);
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
    download->second.quiet.restart();
    library_.setBlobProgress(QString::fromStdString(hash), data.size(),
                             static_cast<qint64>(chunk.total), peer.user->name);
    if (static_cast<quint64>(data.size()) < chunk.total) {
        queue(peer, BlobWant{chunk.hash, static_cast<quint64>(data.size())});
        flush(id);
        return;
    }
    const QByteArray complete = std::move(data);
    downloads_.erase(download);
    library_.endBlobProgress(QString::fromStdString(hash));
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

bool Network::handOver(const std::string &hash, Download &download) {
    const PeerId from = download.from;
    const auto other = std::ranges::find_if(peers_, [&](const auto &entry) {
        const Peer &peer = *entry.second;
        return entry.first != from && peer.ready && peer.user && blobSharedWith(peer, hash);
    });
    if (other == peers_.end())
        return false;
    logInfo(QStringLiteral("sync"), QStringLiteral("%1: goes on fetching a PDF from %2")
                                        .arg(peerLabel(from), peerLabel(other->first)));
    download.from = other->first;
    download.quiet.restart();
    queue(*other->second, BlobWant{rawHash(hash), static_cast<quint64>(download.data.size())});
    flush(other->first);
    return true;
}

// A peer left: its downloads go on from another peer that shares a board showing them, from
// where they stopped, or end.
void Network::dropDownloadsFrom(const PeerId &id) {
    for (auto it = downloads_.begin(); it != downloads_.end();) {
        if (it->second.from != id || handOver(it->first, it->second)) {
            ++it;
            continue;
        }
        library_.endBlobProgress(QString::fromStdString(it->first));
        it = downloads_.erase(it);
    }
}

// A sender that went quiet may be gone without its connection saying so yet: after a while
// its downloads go to someone else, when there is someone else.
void Network::checkStalledDownloads() {
    constexpr qint64 quietLimitMs = 15000;
    if (downloads_.empty()) {
        downloadTimer_.stop();
        return;
    }
    for (auto &[hash, download] : downloads_) {
        if (download.quiet.elapsed() > quietLimitMs)
            handOver(hash, download);
    }
}

} // namespace peerchalk::sync
