#include "hosted/hosted_bridge.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

#include <QJsonDocument>

EM_JS(void, peerchalk_hosted_post, (const char *text),
      { peerchalkHosted.post(UTF8ToString(text)); });
EM_JS_DEPS(peerchalk_hosted, "$UTF8ToString");
#endif

namespace peerchalk::hosted {
namespace {

#ifdef __EMSCRIPTEN__
class PageBridge final : public HostedBridge {
  public:
    QList<QJsonObject> take() override {
        QList<QJsonObject> messages;
        while (true) {
            const QByteArray text(emscripten_run_script_string("peerchalkHosted.next()"));
            if (text.isEmpty())
                return messages;
            messages.append(QJsonDocument::fromJson(text).object());
        }
    }
    void post(const QJsonObject &message) override {
        peerchalk_hosted_post(QJsonDocument(message).toJson(QJsonDocument::Compact).constData());
    }
};
#endif

} // namespace

bool hostedPage() {
#ifdef __EMSCRIPTEN__
    return emscripten_run_script_int("globalThis.peerchalkHosted ? 1 : 0") != 0;
#else
    return false;
#endif
}

std::unique_ptr<HostedBridge> pageBridge() {
#ifdef __EMSCRIPTEN__
    return std::make_unique<PageBridge>();
#else
    return nullptr;
#endif
}

} // namespace peerchalk::hosted
