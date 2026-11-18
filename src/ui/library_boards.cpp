// Library: the live board documents and their saving.
#include "ui/library.hpp"

#include <QTimer>

#include <stdexcept>

namespace peerchalk {

Library::~Library() { saveBoards(); }

BoardDocument &Library::document(const QUuid &boardId) {
    if (const auto it = documents_.find(boardId); it != documents_.end())
        return *it->second;
    const QByteArray saved = workspace_.boardState(boardId); // throws for unknown boards
    auto *document = new BoardDocument(this);
    documents_.emplace(boardId, document);
    if (!saved.isEmpty()) {
        const auto *data = reinterpret_cast<const std::byte *>(saved.constData());
        if (!document->board().apply(ByteView(data, static_cast<std::size_t>(saved.size()))))
            throw std::runtime_error("The saved board is damaged");
    }
    savedRevisions_[boardId] = document->revision(); // loading is not an edit
    document->setDraftListener([this, boardId](const Stroke &stroke, DraftState state) {
        if (draftListener_)
            draftListener_(boardId, stroke, state);
    });
    connect(document, &BoardDocument::changed, this, [this, boardId] {
        if (documents_.at(boardId)->revision() != savedRevisions_[boardId])
            saveTimer_->start();
        emit boardChanged(boardId);
    });
    return *document;
}

BoardDocument *Library::openBoard(const QString &boardId) {
    BoardDocument *result = nullptr;
    attempt([&] {
        const QUuid id = QUuid::fromString(boardId);
        if (!workspace_.hasBoard(id))
            throw std::invalid_argument("Unknown board");
        result = &document(id);
    });
    return result;
}

void Library::saveBoards() {
    saveTimer_->stop();
    for (const auto &[id, document] : documents_) {
        if (document->revision() == savedRevisions_[id] || !workspace_.hasBoard(id))
            continue;
        savedRevisions_[id] = document->revision();
        const Bytes state = document->board().encodeState();
        saveBoardState(id.toString(QUuid::WithoutBraces),
                       QByteArray(reinterpret_cast<const char *>(state.data()),
                                  static_cast<qsizetype>(state.size())));
    }
}

// Drops the documents of boards that were removed or can no longer be opened.
void Library::closeRemovedBoards() {
    for (auto it = documents_.begin(); it != documents_.end();) {
        const bool gone = !workspace_.hasBoard(it->first) || workspace_.board(it->first).deleted();
        if (!gone) {
            ++it;
            continue;
        }
        savedRevisions_.erase(it->first);
        it->second->deleteLater();
        it = documents_.erase(it);
    }
}

} // namespace peerchalk
