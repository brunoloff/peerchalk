#include "ui/library.hpp"

#include <QAbstractItemModel>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlextensionplugin.h>

#include <gtest/gtest.h>

Q_IMPORT_QML_PLUGIN(PeerChalkPlugin)

namespace peerchalk {
namespace {

TEST(MainWindowLifetimeTest, ClosingWithAnOpenBoardKeepsItsChanges) {
    QTemporaryDir dir;
    qputenv("PEERCHALK_DATA_DIR", dir.path().toUtf8());
    QString boardId;
    {
        QQmlApplicationEngine engine;
        engine.loadFromModule("PeerChalk", "Main");
        ASSERT_EQ(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window));
        auto *library = engine.singletonInstance<Library *>("PeerChalk", "Library");
        library->createProject(QStringLiteral("Project"));
        boardId = library->createBoard(QStringLiteral("Board"));
        QMetaObject::invokeMethod(window, "openBoard", Q_ARG(QString, boardId));
        ASSERT_TRUE(QTest::qWaitFor(
            [&] { return window->findChild<QQuickItem *>("boardPage") != nullptr; }));
        Stroke stroke(0xFF000000U, 2.0F);
        stroke.addPoint({0.0F, 0.0F, 1.0F});
        stroke.addPoint({5.0F, 5.0F, 1.0F});
        library->openBoard(boardId)->board().addStroke(stroke);
    } // destroyed with the board open and its change not yet saved
    qunsetenv("PEERCHALK_DATA_DIR");
    Library reopened(dir.path());
    EXPECT_EQ(reopened.openBoard(boardId)->board().strokes().size(), 1U);
}

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
    QQuickItem *openNewBoard(bool pages = false) {
        library_->createProject(QStringLiteral("Project"));
        const QString boardId =
            pages ? library_->createPageBoard(QStringLiteral("Board"), "A4", false, "ruled")
                  : library_->createBoard(QStringLiteral("Board"));
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

TEST_F(MainWindowTest, OwnersSeeNoAccessNoticeOnTheirBoards) {
    QQuickItem *page = openNewBoard();
    ASSERT_NE(page, nullptr);
    EXPECT_EQ(page->property("access").toString(), QStringLiteral("edit"));
    auto *notice = page->findChild<QQuickItem *>(QStringLiteral("boardAccessNotice"));
    ASSERT_NE(notice, nullptr);
    EXPECT_FALSE(notice->isVisible());
}

TEST_F(MainWindowTest, ProjectHeaderCountsNewMembers) {
    QTemporaryDir other;
    Library friendLibrary(other.path());
    ASSERT_TRUE(library_->addContact(friendLibrary.contactCode()));
    const QString project = library_->createProject(QStringLiteral("Project"));
    auto *subtitle = window_->findChild<QObject *>(QStringLiteral("projectSubtitle"));
    ASSERT_NE(subtitle, nullptr);
    EXPECT_EQ(subtitle->property("text").toString(), QStringLiteral("Only you so far"));
    library_->setProjectRole(project, friendLibrary.userKey(), QStringLiteral("member"));
    EXPECT_TRUE(QTest::qWaitFor(
        [&] { return subtitle->property("text").toString() == QStringLiteral("2 people"); }));
}

TEST_F(MainWindowTest, LoadsWithoutQmlWarnings) {
    openNewBoard();
    for (const QQmlError &warning : warnings_)
        ADD_FAILURE() << warning.toString().toStdString();
}

// Opens every dialog on the current screens, then reports the QML warnings seen.
void openEveryDialog(QQuickWindow *window) {
    // Guarded: views may delete their delegates while the dialogs open and close.
    QList<QPointer<QObject>> objects;
    for (QObject *object : window->findChildren<QObject *>())
        objects.append(object);
    for (const QPointer<QObject> &object : objects) {
        if (object.isNull())
            continue;
        const QMetaObject *type = object->metaObject();
        if (!QByteArray(type->className()).contains("Dialog") || type->indexOfMethod("open()") < 0)
            continue;
        QMetaObject::invokeMethod(object, "open");
        QTest::qWait(50);
        QMetaObject::invokeMethod(object, "close");
    }
}

TEST_F(MainWindowTest, PhonesGetABarOnTopAndToolsAlongTheBottom) {
    QQuickItem *page = openNewBoard(true);
    ASSERT_NE(page, nullptr);
    window_->resize(390, 844);
    ASSERT_TRUE(QTest::qWaitFor([&] { return page->property("phone").toBool(); }));
    EXPECT_TRUE(item("phoneChrome")->isVisible());
    EXPECT_FALSE(item("columnRuler")->isVisible());
    auto *canvas = page->findChild<QQuickItem *>(QStringLiteral("boardCanvas"));
    EXPECT_GT(canvas->property("topInset").toReal(), 80.0); // below the bar and its tabs
    window_->resize(1280, 800);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !page->property("phone").toBool(); }));
    EXPECT_FALSE(item("phoneChrome")->isVisible());
    EXPECT_TRUE(item("columnRuler")->isVisible());
    for (const QQmlError &warning : warnings_)
        ADD_FAILURE() << warning.toString().toStdString();
}

TEST_F(MainWindowTest, PageBoardsShowTheirColumnsAndChangeThroughTheCanvas) {
    QQuickItem *page = openNewBoard(true);
    ASSERT_NE(page, nullptr);
    QQuickItem *ruler = item("columnRuler");
    ASSERT_NE(ruler, nullptr);
    EXPECT_TRUE(ruler->isVisible());
    auto *canvas = page->findChild<QQuickItem *>(QStringLiteral("boardCanvas"));
    ASSERT_NE(canvas, nullptr);
    EXPECT_TRUE(canvas->property("pageBoard").toBool());
    auto *items = canvas->property("pageItems").value<QAbstractItemModel *>();
    ASSERT_NE(items, nullptr);
    EXPECT_GE(items->rowCount(), 3); // the heading, the page's number and its gaps
    const QVariantList columns = canvas->property("pageColumns").toList();
    ASSERT_EQ(columns.size(), 1);
    EXPECT_EQ(columns[0].toMap()["paper"].toString(), QStringLiteral("A4"));
    QMetaObject::invokeMethod(canvas, "addColumn", Q_ARG(QString, QString()));
    EXPECT_EQ(canvas->property("pageColumns").toList().size(), 2);
    openEveryDialog(window_);
    for (const QQmlError &warning : warnings_)
        ADD_FAILURE() << warning.toString().toStdString();
}

TEST_F(MainWindowTest, NewBoardsArePageBoardsUnlessChosenOtherwise) {
    library_->createProject(QStringLiteral("Project"));
    auto *dialog = window_->findChild<QObject *>(QStringLiteral("newBoardDialog"));
    ASSERT_NE(dialog, nullptr);
    auto *pages = dialog->findChild<QQuickItem *>(QStringLiteral("pagesKind"));
    auto *infinite = dialog->findChild<QQuickItem *>(QStringLiteral("infiniteKind"));
    ASSERT_NE(pages, nullptr);
    ASSERT_NE(infinite, nullptr);
    infinite->setProperty("checked", true); // a choice left from last time
    QMetaObject::invokeMethod(dialog, "ask", Q_ARG(QString, QStringLiteral("Board")));
    EXPECT_TRUE(dialog->property("pages").toBool());
    ASSERT_TRUE(QTest::qWaitFor([&] { return pages->width() > 0 && infinite->width() > 0; }));
    window_->resize(1280, 800);
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return pages->mapToScene({}).x() < infinite->mapToScene({}).x(); // Pages on the left
    }));
    QMetaObject::invokeMethod(dialog, "close");
}

TEST_F(MainWindowTest, TheSidebarShowsTheVersionBesideTheName) {
    library_->createProject(QStringLiteral("Project"));
    QQuickItem *version = nullptr;
    ASSERT_TRUE(QTest::qWaitFor([&] {
        version = item("appVersion");
        return version != nullptr && version->isVisible() && version->width() > 0;
    }));
    EXPECT_TRUE(QRegularExpression(QStringLiteral("^\\d+\\.\\d+(\\.\\d+)?$"))
                    .match(version->property("text").toString())
                    .hasMatch());
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

TEST_F(MainWindowTest, TheNetworkPageListsServersBesideAMapOfConnections) {
    ASSERT_TRUE(QTest::qWaitFor([this] { return item("connectionChips") != nullptr; }));
    EXPECT_TRUE(item("connectionChips")->isVisible());
    window_->resize(1440, 900);
    QMetaObject::invokeMethod(window_, "openSettings", Q_ARG(int, 1));
    ASSERT_TRUE(QTest::qWaitFor([this] { return item("networkMap") != nullptr; }));
    ASSERT_TRUE(QTest::qWaitFor([this] { return !item("stack")->property("busy").toBool(); }));
    EXPECT_TRUE(item("discoveryServers")->isVisible());
    EXPECT_TRUE(item("firewallServers")->isVisible());
    EXPECT_EQ(item("discoveryServers")->property("servers").toList().size(), 4)
        << "two DHT nodes, then two Nostr relays for browsers";
    EXPECT_EQ(item("firewallServers")->property("servers").toList().size(), 3)
        << "two STUN servers and PeerChalk's relay";
    const QQuickItem *map = item("networkMap");
    EXPECT_GT(map->mapToScene({}).x(), item("discoveryServers")->mapToScene({}).x())
        << "wide windows show the map beside the settings";
    window_->resize(1000, 800);
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return map->mapToScene({}).y() < item("discoveryServers")->mapToScene({}).y();
    })) << "narrower windows show it above them";
    for (const QQmlError &warning : warnings_)
        ADD_FAILURE() << warning.toString().toStdString();
}

// Twelve devices: nine direct (one with an unknown route), two relayed, one connecting.
QVariantList crowd() {
    QVariantList devices;
    const QStringList names{"Ana", "Ben",  "Chloé", "Dev", "Eli",   "Farah",
                            "Gus", "Hana", "Ivo",   "Sam", "Tomás", "Yara"};
    for (int i = 0; i < names.size(); ++i) {
        const QString route = i < 8    ? "direct"
                              : i == 8 ? "unknown"
                              : i < 11 ? "relay"
                                       : "connecting";
        devices.push_back(QVariantMap{{"device", QString::number(i)},
                                      {"name", names[i]},
                                      {"colorIndex", i},
                                      {"route", route}});
    }
    return devices;
}

TEST_F(MainWindowTest, TheConnectionMapGroupsManyDevicesByRoute) {
    window_->resize(1440, 900);
    QMetaObject::invokeMethod(window_, "openSettings", Q_ARG(int, 1));
    ASSERT_TRUE(QTest::qWaitFor([this] { return item("networkMap") != nullptr; }));
    QQuickItem *map = item("networkMap");
    QQmlProperty::write(
        map, QStringLiteral("methods"),
        QVariantList{QVariantMap{{"kind", "dht"}, {"state", "working"}, {"count", 2}},
                     QVariantMap{{"kind", "nostr"}, {"state", "working"}, {"count", 2}},
                     QVariantMap{{"kind", "mqtt"}, {"state", "unreachable"}, {"count", 1}},
                     QVariantMap{{"kind", "peerjs"}, {"state", "checking"}, {"count", 1}}});
    QQmlProperty::write(map, QStringLiteral("devices"), crowd());
    EXPECT_EQ(map->property("direct").toList().size(), 9);
    EXPECT_EQ(map->property("relayed").toList().size(), 2);
    EXPECT_EQ(map->property("connecting").toList().size(), 1);
    const QQuickItem *showAll = item("showAllDevices");
    ASSERT_NE(showAll, nullptr);
    EXPECT_TRUE(showAll->isVisible()) << "beyond six rows, the rest fold away";
    window_->resize(1000, 900);
    ASSERT_TRUE(QTest::qWaitFor([&] { return map->property("wide").toBool(); }));
    EXPECT_FALSE(showAll->isVisible()) << "wide maps show avatars instead of rows";
    for (const QQmlError &warning : warnings_)
        ADD_FAILURE() << warning.toString().toStdString();
}

TEST_F(MainWindowTest, ThePaletteShowsCustomColoursBeforeTheAddButton) {
    openNewBoard();
    QQuickItem *add = item("customColorButton");
    ASSERT_NE(add, nullptr);
    EXPECT_TRUE(add->isVisible());
    const qreal before = add->y() * 1000 + add->x();
    library_->addCustomColor(QStringLiteral("#123456"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return add->y() * 1000 + add->x() > before; }))
        << "the new colour should take the add button's place";
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
