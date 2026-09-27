//
// Custom gtest main providing a QCoreApplication instance, shared by the app,
// qt_data and ui suites. Their QObject/signal view models and SessionController,
// QSettings/QDir/QFileSystemWatcher use, and QSignalSpy / QTest::qWaitFor all
// need one to behave correctly in a headless test binary.
//
// ui_tests defines KSV_GTEST_GUI_APP: it instantiates QML components built from
// Qt Quick Controls, which crash without a QGuiApplication. It defaults to the
// same offscreen platform and Fusion style the QML suites use.
//
// The integration suite keeps its own main: it needs a QGuiApplication, the QML
// type registration, and a temp-dir QSettings redirect.
//

#include <QCoreApplication>
#include <gtest/gtest.h>

#ifdef KSV_GTEST_GUI_APP
#include <QGuiApplication>
#endif

int main(int argc, char **argv) {
#ifdef KSV_GTEST_GUI_APP
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    QGuiApplication app(argc, argv);
#else
    QCoreApplication app(argc, argv);
#endif
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
