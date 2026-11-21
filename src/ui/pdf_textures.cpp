#include "ui/pdf_textures.hpp"

#include <QQuickWindow>
#include <QSGTexture>

namespace peerchalk {
namespace {

constexpr int keptIdleFrames = 30; // about half a second at 60 frames a second

} // namespace

PdfTextureNode::PdfTextureNode() = default;
PdfTextureNode::~PdfTextureNode() = default;

QSGTexture *PdfTextureNode::texture(QQuickWindow &window, const QImage &image) {
    Entry &entry = textures_[image.cacheKey()];
    if (!entry.texture) {
        entry.texture.reset(window.createTextureFromImage(image));
        if (entry.texture)
            entry.texture->setFiltering(QSGTexture::Linear);
    }
    entry.idleFrames = -1; // becomes 0 at the end of this frame
    return entry.texture.get();
}

void PdfTextureNode::endFrame() {
    std::erase_if(textures_, [](auto &item) { return ++item.second.idleFrames > keptIdleFrames; });
}

} // namespace peerchalk
