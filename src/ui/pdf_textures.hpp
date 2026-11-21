#pragma once

#include <QImage>
#include <QSGNode>

#include <memory>
#include <unordered_map>

class QQuickWindow;
class QSGTexture;

namespace peerchalk {

// The textures of rendered PDF pages that a canvas draws, kept while they are in use. It is
// the canvas's root scene node, so the scene graph deletes it, and the textures, on its own
// thread; only updatePaintNode touches it.
class PdfTextureNode : public QSGNode {
  public:
    PdfTextureNode();
    ~PdfTextureNode() override;

    // The image's texture, made the first time it is asked for.
    [[nodiscard]] QSGTexture *texture(QQuickWindow &window, const QImage &image);
    // Releases the textures not asked for in the last few frames.
    void endFrame();
    [[nodiscard]] std::size_t size() const { return textures_.size(); }

  private:
    struct Entry {
        std::unique_ptr<QSGTexture> texture;
        int idleFrames;
    };

    std::unordered_map<qint64, Entry> textures_;
};

} // namespace peerchalk
