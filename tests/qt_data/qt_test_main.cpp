//
// Custom gtest main providing a QCoreApplication instance, required for
// QSettings/QDir/QFileSystemWatcher and Qt's event loop (QTest::qWaitFor) to
// behave correctly in a headless test binary.
//

#include <QCoreApplication>
#include <gtest/gtest.h>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
