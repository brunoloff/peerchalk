#include "app/app_controller.hpp"
#include "ui/app_style.hpp"
#include "ui/library.hpp"

#include <QGuiApplication>
#include <QSurfaceFormat>
#include <QtQml/qqmlextensionplugin.h>

#include <exception>
#include <memory>

Q_IMPORT_QML_PLUGIN(PeerChalkPlugin)

int main(int argc, char **argv) {
#ifdef __EMSCRIPTEN__
    // web_storage.js keeps this folder in the browser's IndexedDB.
    qputenv("PEERCHALK_DATA_DIR", "/peerchalk");
    qunsetenv("USER"); // Emscripten's placeholder is not a name worth showing to others
#endif
    // Multisampling smooths the edges of strokes, which are drawn as plain triangles.
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("PeerChalk"));
    peerchalk::useAppControlsStyle();
    std::unique_ptr<peerchalk::InstanceSlot> slot;
    try {
        slot = peerchalk::InstanceSlot::claim(peerchalk::Library::defaultDataDirectory());
    } catch (const std::exception &error) {
        qFatal("PeerChalk cannot use its data folder: %s", error.what());
    }
    // After the application, so that its windows are gone before the application is.
    peerchalk::AppController controller(*slot);
    try {
        controller.start(argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString());
    } catch (const std::exception &error) {
        qFatal("PeerChalk cannot read its data: %s", error.what());
    }
    // Save and close connections before the engine and its singletons go away.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controller,
                     &peerchalk::AppController::prepareToQuit);
    return app.exec();
}
