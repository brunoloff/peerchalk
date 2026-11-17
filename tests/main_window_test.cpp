#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

#include <gtest/gtest.h>

Q_IMPORT_QML_PLUGIN(PeerChalkPlugin)

namespace {

class MainWindowTest : public ::testing::Test {
  protected:
    void SetUp() override {
        QObject::connect(&engine_, &QQmlEngine::warnings,
                         [this](const QList<QQmlError> &list) { warnings_.append(list); });
        engine_.loadFromModule("PeerChalk", "Main");
        ASSERT_EQ(engine_.rootObjects().size(), 1);
        window_ = qobject_cast<QQuickWindow *>(engine_.rootObjects().front());
        ASSERT_NE(window_, nullptr);
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_));
    }

    [[nodiscard]] QQuickItem *item(const char *name) const {
        return window_->findChild<QQuickItem *>(QString::fromLatin1(name));
    }

    // True once the side panel has finished sliding fully into view.
    [[nodiscard]] bool panelOpen() const {
        const QQuickItem *panel = item("inspector");
        return panel->isVisible() && qFuzzyCompare(panel->x() + panel->width(), window_->width());
    }

    QQmlApplicationEngine engine_;
    QList<QQmlError> warnings_;
    QQuickWindow *window_{nullptr};
};

TEST_F(MainWindowTest, LoadsWithoutQmlWarnings) {
    for (const QQmlError &warning : warnings_)
        ADD_FAILURE() << warning.toString().toStdString();
}

TEST_F(MainWindowTest, PanelIsDockedWithTheMinimapAtItsBottom) {
    EXPECT_TRUE(panelOpen());
    EXPECT_TRUE(item("panelNavigator")->isVisible());
    EXPECT_FALSE(item("floatingNavigator")->isVisible());
}

TEST_F(MainWindowTest, AutoHideHidesThePanelAndFloatsTheMinimap) {
    window_->setProperty("inspectorAutoHide", true);
    window_->setProperty("inspectorRevealed", false);
    ASSERT_TRUE(QTest::qWaitFor([this] { return !item("inspector")->isVisible(); }));
    EXPECT_TRUE(item("floatingNavigator")->isVisible());
}

TEST_F(MainWindowTest, TappingTheEdgeTabRevealsTheHiddenPanel) {
    window_->setProperty("inspectorAutoHide", true);
    window_->setProperty("inspectorRevealed", false);
    ASSERT_TRUE(QTest::qWaitFor([this] { return !item("inspector")->isVisible(); }));
    QMetaObject::invokeMethod(window_, "revealInspector", Q_ARG(bool, true));
    ASSERT_TRUE(QTest::qWaitFor([this] { return panelOpen(); }));
    EXPECT_FALSE(item("floatingNavigator")->isVisible());
    QTest::qWait(800);
    EXPECT_TRUE(panelOpen()) << "a panel opened by tapping must stay open";
}

TEST_F(MainWindowTest, PanelRevealedByHoverHidesAgain) {
    window_->setProperty("inspectorAutoHide", true);
    QMetaObject::invokeMethod(window_, "revealInspector", Q_ARG(bool, false));
    ASSERT_TRUE(QTest::qWaitFor([this] { return panelOpen(); }));
    ASSERT_TRUE(QTest::qWaitFor([this] { return !item("inspector")->isVisible(); }, 3000));
}

} // namespace
