//
// Custom gtest main providing a QCoreApplication instance, shared by the app,
// qt_data and ui suites. Their QObject/signal view models and SessionController,
// QSettings/QDir/QFileSystemWatcher use, and QSignalSpy / QTest::qWaitFor all
// need one to behave correctly in a headless test binary.
//
// The integration suite keeps its own main: it needs a QGuiApplication, the QML
// type registration, and a temp-dir QSettings redirect.
//

#include <QCoreApplication>
#include <gtest/gtest.h>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
