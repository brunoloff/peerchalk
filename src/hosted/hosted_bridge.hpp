#pragma once

#include <QJsonObject>
#include <QList>

#include <memory>

namespace peerchalk::hosted {

// The page's link to the dashboard that hosts it: window.parent.postMessage, through
// web_hosted.js. Tests use their own.
class HostedBridge {
  public:
    virtual ~HostedBridge() = default;
    // Messages that arrived since the last call, oldest first.
    [[nodiscard]] virtual QList<QJsonObject> take() = 0;
    virtual void post(const QJsonObject &message) = 0;
};

// Whether this page was opened to be hosted (its address has ?hosted=1); false outside
// browsers.
[[nodiscard]] bool hostedPage();
// The page's bridge; nullptr outside browsers.
[[nodiscard]] std::unique_ptr<HostedBridge> pageBridge();

} // namespace peerchalk::hosted
