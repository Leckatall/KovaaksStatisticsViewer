//
// Custom gtest main providing a QCoreApplication instance, required for
// QObject/signal-based classes (SessionController) used in these tests.
//

#include <QCoreApplication>
#include <gtest/gtest.h>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
