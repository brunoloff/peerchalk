#include "ui/library.hpp"

#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

#include <gtest/gtest.h>

Q_IMPORT_QML_PLUGIN(PeerChalkPlugin)

namespace peerchalk {
namespace {

// Loads the whole app with its data in a fresh temporary folder.
class MainWindowTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ASSERT_TRUE(dataDir_.isValid());
        qputenv("PEERCHALK_DATA_DIR", dataDir_.path().toUtf8());
        QObject::connect(&engine_, &QQmlEngine::warnings,
                         [this](const QList<QQmlError> &list) { warnings_.append(list); });
        engine_.loadFromModule("PeerChalk", "Main");
        ASSERT_EQ(engine_.rootObjects().size(), 1);
        window_ = qobject_cast<QQuickWindow *>(engine_.rootObjects().front());
        ASSERT_NE(window_, nullptr);
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_));
        library_ = engine_.singletonInstance<Library *>("PeerChalk", "Library");
        ASSERT_NE(library_, nullptr);
    }

    void TearDown() override { qunsetenv("PEERCHALK_DATA_DIR"); }

    [[nodiscard]] QQuickItem *item(const char *name) const {
        return window_->findChild<QQuickItem *>(QString::fromLatin1(name));
    }

    // Creates a project and a board in it, opens the board and returns its page.
    QQuickItem *openNewBoard() {
        library_->createProject(QStringLiteral("Project"));
        const QString boardId = library_->createBoard(QStringLiteral("Board"));
        QMetaObject::invokeMethod(window_, "openBoard", Q_ARG(QString, boardId));
        QQuickItem *page = nullptr;
        // Wait for the page to exist and the stack to finish its transition; the stack
        // ignores input while it is busy.
        EXPECT_TRUE(QTest::qWaitFor([&] {
            page = item("boardPage");
            return page != nullptr && !item("stack")->property("busy").toBool();
        }));
        return page;
    }

    // True once the board page's side panel has finished sliding fully into view.
    [[nodiscard]] bool panelOpen() const {
        const QQuickItem *panel = item("inspector");
        return panel != nullptr && panel->isVisible() &&
               qFuzzyCompare(panel->x() + panel->width(), item("boardPage")->width());
    }

    [[nodiscard]] bool panelHidden() const {
        const QQuickItem *panel = item("inspector");
        return panel != nullptr && !panel->isVisible();
    }

    QTemporaryDir dataDir_;
    QQmlApplicationEngine engine_;
    QList<QQmlError> warnings_;
    QQuickWindow *window_{nullptr};
    Library *library_{nullptr};
};

TEST_F(MainWindowTest, LoadsWithoutQmlWarnings) {
    openNewBoard();
    for (const QQmlError &warning : warnings_)
        ADD_FAILURE() << warning.toString().toStdString();
}

// Opens every dialog on the current screens, then reports the QML warnings seen.
void openEveryDialog(QQuickWindow *window) {
    for (QObject *object : window->findChildren<QObject *>()) {
        const QMetaObject *type = object->metaObject();
        if (!QByteArray(type->className()).contains("Dialog") || type->indexOfMethod("open()") < 0)
            continue;
        QMetaObject::invokeMethod(object, "open");
        QTest::qWait(50);
        QMetaObject::invokeMethod(object, "close");
    }
}

TEST_F(MainWindowTest, UsesTheBasicControlsStyle) {
    EXPECT_EQ(QQuickStyle::name(), QStringLiteral("Basic"));
}

TEST_F(MainWindowTest, DialogsOpenWithoutQmlWarnings) {
    library_->createProject(QStringLiteral("Project"));
    openEveryDialog(window_);
    QMetaObject::invokeMethod(window_, "openSettings", Q_ARG(int, 0));
    ASSERT_TRUE(QTest::qWaitFor([this] { return !item("stack")->property("busy").toBool(); }));
    openEveryDialog(window_);
    item("settingsPage")->setProperty("currentTab", 1);
    QTest::qWait(50);
    for (const QQmlError &warning : warnings_)
        ADD_FAILURE() << warning.toString().toStdString();
}

TEST_F(MainWindowTest, TheSettingsPageOpensOnTheChosenTab) {
    QMetaObject::invokeMethod(window_, "openSettings", Q_ARG(int, 1));
    ASSERT_TRUE(QTest::qWaitFor([this] { return item("networkPane") != nullptr; }));
    EXPECT_TRUE(item("networkPane")->isVisible());
    item("settingsPage")->setProperty("currentTab", 0);
    EXPECT_FALSE(item("networkPane")->isVisible());
}

TEST_F(MainWindowTest, StartsWithAnIdentityAndNoProjects) {
    EXPECT_FALSE(library_->displayName().isEmpty());
    EXPECT_FALSE(library_->fingerprint().isEmpty());
    EXPECT_EQ(library_->projects()->rowCount(), 0);
    EXPECT_EQ(item("boardPage"), nullptr);
}

TEST_F(MainWindowTest, BoardPanelIsDockedWithTheMinimapAtItsBottom) {
    openNewBoard();
    EXPECT_TRUE(QTest::qWaitFor([this] { return panelOpen(); }));
    EXPECT_TRUE(item("panelNavigator")->isVisible());
    EXPECT_FALSE(item("floatingNavigator")->isVisible());
}

TEST_F(MainWindowTest, AutoHideHidesThePanelAndFloatsTheMinimap) {
    QQuickItem *page = openNewBoard();
    page->setProperty("inspectorAutoHide", true);
    page->setProperty("inspectorRevealed", false);
    ASSERT_TRUE(QTest::qWaitFor([this] { return panelHidden(); }));
    EXPECT_TRUE(item("floatingNavigator")->isVisible());
}

TEST_F(MainWindowTest, TappingTheEdgeTabRevealsTheHiddenPanel) {
    QQuickItem *page = openNewBoard();
    page->setProperty("inspectorAutoHide", true);
    page->setProperty("inspectorRevealed", false);
    ASSERT_TRUE(QTest::qWaitFor([this] { return panelHidden(); }));
    QMetaObject::invokeMethod(page, "revealInspector", Q_ARG(bool, true));
    ASSERT_TRUE(QTest::qWaitFor([this] { return panelOpen(); }));
    EXPECT_FALSE(item("floatingNavigator")->isVisible());
    QTest::qWait(800);
    EXPECT_TRUE(panelOpen()) << "a panel opened by tapping must stay open";
}

TEST_F(MainWindowTest, PanelRevealedByHoverHidesAgain) {
    QQuickItem *page = openNewBoard();
    page->setProperty("inspectorAutoHide", true);
    QMetaObject::invokeMethod(page, "revealInspector", Q_ARG(bool, false));
    ASSERT_TRUE(QTest::qWaitFor([this] { return panelOpen(); }));
    ASSERT_TRUE(QTest::qWaitFor([this] { return panelHidden(); }, 3000));
}

TEST_F(MainWindowTest, DrawingIsSavedWhenLeavingTheBoard) {
    QQuickItem *page = openNewBoard();
    const QString boardId = page->property("boardId").toString();
    QTest::mousePress(window_, Qt::LeftButton, {}, {400, 300});
    QTest::mouseMove(window_, {450, 320});
    QTest::mouseRelease(window_, Qt::LeftButton, {}, {500, 340});
    QMetaObject::invokeMethod(page, "back");
    ASSERT_TRUE(QTest::qWaitFor([this] { return item("boardPage") == nullptr; }));
    EXPECT_FALSE(library_->boardState(boardId).isEmpty());
}

} // namespace
} // namespace peerchalk
