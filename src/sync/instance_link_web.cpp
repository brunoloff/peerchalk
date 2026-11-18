#include "sync/instance_link.hpp"

#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>

#include <utility>

namespace {

peerchalk::sync::InstanceLink *activeLink = nullptr;
QByteArray incoming;

EM_JS(void, peerchalk_link_open, (), { peerchalkLinkOpen(); });
EM_JS(void, peerchalk_link_post, (const char *data, unsigned size),
      { peerchalkLinkPost(data, size); });

} // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE char *peerchalk_link_buffer(unsigned size) {
    incoming.resize(static_cast<qsizetype>(size));
    return incoming.data();
}

EMSCRIPTEN_KEEPALIVE void peerchalk_link_ready() {
    if (activeLink != nullptr)
        emit activeLink->received(std::exchange(incoming, {}));
}
}

namespace peerchalk::sync {
namespace {

class BrowserLink final : public InstanceLink {
  public:
    BrowserLink() {
        activeLink = this;
        peerchalk_link_open();
    }
    ~BrowserLink() override { activeLink = nullptr; }

    void send(const QByteArray &message) override {
        peerchalk_link_post(message.constData(), static_cast<unsigned>(message.size()));
    }
};

} // namespace

std::unique_ptr<InstanceLink> InstanceLink::create(const InstanceSlot &) {
    return std::make_unique<BrowserLink>();
}

} // namespace peerchalk::sync
