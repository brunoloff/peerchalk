#include <QGuiApplication>

#include <gtest/gtest.h>

// GoogleTest entry point for tests that need a Qt GUI application.
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
