#include "app/smoke_test.hpp"

#include <QGuiApplication>
#include <QImage>
#include <QQuickWindow>
#include <QTextStream>
#include <QTimer>

#include <chrono>

namespace peerchalk {
namespace {

constexpr auto settleTime = std::chrono::seconds(5);

QQuickWindow *shownWindow() {
    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        if (auto *quick = qobject_cast<QQuickWindow *>(window); quick && quick->isVisible())
            return quick;
    }
    return nullptr;
}

void finish(const QString &screenshot) {
    QQuickWindow *window = shownWindow();
    if (window == nullptr) {
        QTextStream(stderr) << "PeerChalk smoke test: no window appeared\n";
        QCoreApplication::exit(2);
        return;
    }
    const QImage image = window->grabWindow();
    if (image.isNull() || !image.save(screenshot)) {
        QTextStream(stderr) << "PeerChalk smoke test: could not save " << screenshot << '\n';
        QCoreApplication::exit(3);
        return;
    }
    QTextStream(stdout) << "PeerChalk started (" << image.width() << 'x' << image.height()
                        << ")\n";
    QCoreApplication::exit(0);
}

} // namespace

void startSmokeTestIfRequested() {
    const QString screenshot = qEnvironmentVariable("PEERCHALK_SMOKE_TEST");
    if (screenshot.isEmpty())
        return;
    QTimer::singleShot(settleTime, qApp, [screenshot] { finish(screenshot); });
}

} // namespace peerchalk
