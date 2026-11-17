#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickWindow>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

#include <gtest/gtest.h>

Q_IMPORT_QML_PLUGIN(PeerChalkPlugin)

namespace {

TEST(MainWindow, LoadsWithoutQmlWarnings) {
    QQmlApplicationEngine engine;
    QList<QQmlError> warnings;
    QObject::connect(&engine, &QQmlEngine::warnings,
                     [&warnings](const QList<QQmlError> &list) { warnings.append(list); });
    engine.loadFromModule("PeerChalk", "Main");
    ASSERT_EQ(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
    ASSERT_NE(window, nullptr);
    ASSERT_TRUE(QTest::qWaitForWindowExposed(window));
    for (const QQmlError &warning : warnings)
        ADD_FAILURE() << warning.toString().toStdString();
}

} // namespace
