#pragma once

// People, devices and boards for the repository tests.
#include "repository/board_loader.hpp"
#include "repository/board_writer.hpp"
#include "repository/encoding.hpp"

#include <QRandomGenerator>
#include <QTemporaryDir>

namespace peerchalk::repository::testing {

// One person's device: its own user and device keys.
struct Device {
    QTemporaryDir dir;
    UserIdentity identity{dir.path()};
    Author author{authorOf(identity)};
    [[nodiscard]] QByteArray user() const { return identity.userKey(); }
    [[nodiscard]] QByteArray key() const { return identity.deviceKey(); }
};

// A new network secret, as an owner who removes someone makes.
inline QByteArray freshSecret() {
    QByteArray secret(32, '\0');
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(secret.data()), 8);
    return secret;
}

inline Stroke line(float x0, float x1) {
    Stroke stroke(0xff000000U, 2.0F);
    stroke.addPoint({x0, 0.0F, 1.0F});
    stroke.addPoint({x1, 0.0F, 1.0F});
    return stroke;
}

// Applies writes and deletions to a board's files, as a commit would.
inline void apply(BoardFiles &files, const FileChanges &changes) {
    for (const auto &[path, bytes] : changes) {
        if (bytes)
            files[path] = *bytes;
        else
            files.erase(path);
    }
}

// The board's document as loaded, ready for edits.
inline std::unique_ptr<Board> documentOf(const LoadedBoard &loaded) {
    auto document = std::make_unique<Board>();
    document->apply(byteView(loaded.state));
    return document;
}

// A device's edit, saved as its change file.
inline void draw(BoardFiles &files, const Device &device, const Stroke &stroke) {
    const LoadedBoard loaded = loadBoard(files, {});
    const std::unique_ptr<Board> document = documentOf(loaded);
    document->addStroke(stroke);
    files[changeFilePath(device.key())] =
        changeFile(*document, loaded, device.author, {document->clientId()});
}

inline bool hasWarning(const LoadedBoard &loaded, const QString &path) {
    return std::ranges::any_of(loaded.warnings,
                               [&](const LoadWarning &warning) { return warning.path == path; });
}

inline std::size_t strokeCount(const LoadedBoard &loaded) {
    return documentOf(loaded)->strokes().size();
}

} // namespace peerchalk::repository::testing
