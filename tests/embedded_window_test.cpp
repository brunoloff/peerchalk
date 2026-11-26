#include "ui/library.hpp"

#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

// Whether any item of the window is a HomePage.
bool hasHomePage(const QQuickWindow &window) {
    const QList<QQuickItem *> items = window.findChildren<QQuickItem *>();
    return std::ranges::any_of(items, [](const QQuickItem *each) {
        return QByteArray(each->metaObject()->className()).startsWith("HomePage");
    });
}

TEST(StandaloneWindowTest, StartsAtTheHomeScreen) {
    QTemporaryDir dir;
    qputenv("PEERCHALK_DATA_DIR", dir.path().toUtf8());
    QQmlApplicationEngine engine;
    engine.loadFromModule("PeerChalk", "Main");
    ASSERT_EQ(engine.rootObjects().size(), 1);
    EXPECT_TRUE(hasHomePage(*qobject_cast<QQuickWindow *>(engine.rootObjects().front())));
    qunsetenv("PEERCHALK_DATA_DIR");
}

// The whole app as an embedding host loads it (HostLink.embedded).
class EmbeddedWindowTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ASSERT_TRUE(dataDir_.isValid());
        qputenv("PEERCHALK_DATA_DIR", dataDir_.path().toUtf8());
        QObject::connect(&engine_, &QQmlEngine::warnings,
                         [this](const QList<QQmlError> &list) { warnings_.append(list); });
        engine_.setProperty("hosted", true);
        engine_.loadFromModule("PeerChalk", "Main");
        ASSERT_EQ(engine_.rootObjects().size(), 1);
        window_ = qobject_cast<QQuickWindow *>(engine_.rootObjects().front());
        ASSERT_NE(window_, nullptr);
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_));
        library_ = engine_.singletonInstance<Library *>("PeerChalk", "Library");
    }

    void TearDown() override { qunsetenv("PEERCHALK_DATA_DIR"); }

    [[nodiscard]] QQuickItem *item(const char *name) const {
        return window_->findChild<QQuickItem *>(QString::fromLatin1(name));
    }

    QTemporaryDir dataDir_;
    QQmlApplicationEngine engine_;
    QList<QQmlError> warnings_;
    QQuickWindow *window_{nullptr};
    Library *library_{nullptr};
};

TEST_F(EmbeddedWindowTest, ThereIsNoHomeScreenOnlyTheBoard) {
    EXPECT_FALSE(hasHomePage(*window_));
    EXPECT_EQ(item("stack")->property("depth").toInt(), 0);
    library_->createProject(QStringLiteral("Project"));
    const QString boardId = library_->createBoard(QStringLiteral("Board"));
    QMetaObject::invokeMethod(window_, "openBoard", Q_ARG(QString, boardId));
    ASSERT_TRUE(QTest::qWaitFor([&] { return item("boardPage") != nullptr; }));
    EXPECT_EQ(item("stack")->property("depth").toInt(), 1);
    EXPECT_FALSE(hasHomePage(*window_));
    EXPECT_EQ(window_->title(), QStringLiteral("Board — PeerChalk"));
    for (const QQmlError &warning : warnings_)
        ADD_FAILURE() << warning.toString().toStdString();
}

TEST_F(EmbeddedWindowTest, PhonesGetTheConnectionButtonAndPaneToo) {
    library_->createProject(QStringLiteral("Project"));
    const QString boardId = library_->createBoard(QStringLiteral("Board"));
    QMetaObject::invokeMethod(window_, "openBoard", Q_ARG(QString, boardId));
    ASSERT_TRUE(QTest::qWaitFor([&] { return item("boardPage") != nullptr; }));
    window_->resize(390, 844);
    ASSERT_TRUE(QTest::qWaitFor([&] { return item("boardPage")->property("phone").toBool(); }));
    QQuickItem *button = item("phoneNetworkButton");
    ASSERT_NE(button, nullptr);
    EXPECT_TRUE(button->isVisible());
    EXPECT_FALSE(item("boardNetworkPane")->isVisible());
    QMetaObject::invokeMethod(button, "clicked");
    EXPECT_TRUE(item("boardNetworkPane")->isVisible());
    EXPECT_EQ(item("boardNetworkPane")->width(), window_->width()); // the whole phone
    for (const QQmlError &warning : warnings_)
        ADD_FAILURE() << warning.toString().toStdString();
}

} // namespace
} // namespace peerchalk
