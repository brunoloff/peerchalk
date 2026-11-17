#include "sync/network.hpp"
#include "ui/app_style.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QSurfaceFormat>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(PeerChalkPlugin)

int main(int argc, char **argv) {
    // Multisampling smooths the edges of strokes, which are drawn as plain triangles.
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("PeerChalk"));
    peerchalk::useAppControlsStyle();
    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("PeerChalk", "Main");
    // Close connections and stop p2pdc before the engine and its singletons go away.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&engine] {
        auto *network =
            engine.singletonInstance<peerchalk::sync::Network *>("PeerChalk", "Network");
        if (network != nullptr)
            network->shutdown(std::chrono::seconds(3));
    });
    return app.exec();
}
