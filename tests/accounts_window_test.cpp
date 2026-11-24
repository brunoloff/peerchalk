#include "library/account_store.hpp"

#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace peerchalk {
namespace {

// Loads the account screen over a data folder that already has accounts.
class AccountsWindowTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ASSERT_TRUE(dataDir_.isValid());
        qputenv("PEERCHALK_DATA_DIR", dataDir_.path().toUtf8());
        AccountStore store(dataDir_.path());
        const QString ana = store.create(QStringLiteral("Ana"));
        const QString ben = store.create(QStringLiteral("Ben"));
        store.markOpened(ana);
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); // timestamps have 1 ms steps
        store.markOpened(ben);
        QObject::connect(&engine_, &QQmlEngine::warnings,
                         [this](const QList<QQmlError> &list) { warnings_.append(list); });
        engine_.loadFromModule("PeerChalk", "AccountsWindow");
        ASSERT_EQ(engine_.rootObjects().size(), 1);
        window_ = qobject_cast<QQuickWindow *>(engine_.rootObjects().front());
        ASSERT_NE(window_, nullptr);
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_));
    }

    void TearDown() override { qunsetenv("PEERCHALK_DATA_DIR"); }

    [[nodiscard]] QQuickItem *item(const char *name) const {
        return window_->findChild<QQuickItem *>(QString::fromLatin1(name));
    }

    QTemporaryDir dataDir_;
    QQmlApplicationEngine engine_;
    QQuickWindow *window_{nullptr};
    QList<QQmlError> warnings_;
};

TEST_F(AccountsWindowTest, OffersTheAccountOpenedLastAsTheLargeButton) {
    QQuickItem *button = item("continueButton");
    ASSERT_NE(button, nullptr);
    EXPECT_TRUE(button->isVisible());
    EXPECT_EQ(button->property("account").toMap().value(QStringLiteral("name")),
              QStringLiteral("Ben"));
    EXPECT_EQ(item("aboutToggle"), nullptr); // a wide window shows the explanation beside it
    EXPECT_TRUE(warnings_.isEmpty()) << warnings_.front().toString().toStdString();
}

TEST_F(AccountsWindowTest, ShowsTheVersionBesideTheNameOnWideAndNarrowWindows) {
    const auto shownVersion = [this] {
        QQuickItem *version = item("welcomeVersion");
        return version != nullptr && version->isVisible() && version->width() > 0 &&
               QRegularExpression(QStringLiteral("^\\d+\\.\\d+(\\.\\d+)?$"))
                   .match(version->property("text").toString())
                   .hasMatch();
    };
    EXPECT_TRUE(QTest::qWaitFor(shownVersion));
    window_->resize(390, 844);
    ASSERT_TRUE(QTest::qWaitFor([&] { return item("aboutToggle") != nullptr; }));
    EXPECT_TRUE(QTest::qWaitFor(shownVersion));
    EXPECT_TRUE(warnings_.isEmpty()) << warnings_.front().toString().toStdString();
}

TEST_F(AccountsWindowTest, NarrowWindowsFoldTheExplanationAway) {
    window_->resize(390, 844);
    ASSERT_TRUE(QTest::qWaitFor([&] { return item("aboutToggle") != nullptr; }));
    ASSERT_NE(item("continueButton"), nullptr);
    EXPECT_TRUE(item("continueButton")->isVisible());
    EXPECT_FALSE(item("aboutToggle")->property("checked").toBool());
    EXPECT_TRUE(warnings_.isEmpty()) << warnings_.front().toString().toStdString();
}

} // namespace
} // namespace peerchalk
