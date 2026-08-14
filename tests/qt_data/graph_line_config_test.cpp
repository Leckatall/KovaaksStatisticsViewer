#include <gtest/gtest.h>

#include <QSettings>
#include <QTemporaryDir>

#include "graph_line_config.h"

using namespace ksv::qt_data;

namespace {
    class GraphLineConfigTest : public testing::Test {
    protected:
        QTemporaryDir tempDir;

        void SetUp() override {
            ASSERT_TRUE(tempDir.isValid());
            QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
        }
    };

    TEST_F(GraphLineConfigTest, ReturnsEmptyWhenUnset) {
        const GraphLineConfig config(QSettings::IniFormat);

        EXPECT_TRUE(config.getDisabledGraphLineKeys().empty());
    }

    TEST_F(GraphLineConfigTest, PersistsOpaqueKeys) {
        {
            GraphLineConfig config(QSettings::IniFormat);
            config.setDisabledGraphLineKeys({"score", "futureColumn"});
        }

        const GraphLineConfig reloaded(QSettings::IniFormat);
        EXPECT_EQ(reloaded.getDisabledGraphLineKeys(),
                  (std::vector<std::string>{"score", "futureColumn"}));
    }

    TEST_F(GraphLineConfigTest, NormalizesDuplicateKeys) {
        GraphLineConfig config(QSettings::IniFormat);

        config.setDisabledGraphLineKeys({"score", "futureColumn", "score", "futureColumn"});

        EXPECT_EQ(config.getDisabledGraphLineKeys(),
                  (std::vector<std::string>{"score", "futureColumn"}));
    }

    TEST_F(GraphLineConfigTest, NotifiesAfterEffectiveChangeOnly) {
        GraphLineConfig config(QSettings::IniFormat);
        int notifications = 0;
        std::vector<std::string> observedKeys;
        config.onDisabledGraphLinesChanged([&] {
            ++notifications;
            observedKeys = config.getDisabledGraphLineKeys();
        });

        config.setDisabledGraphLineKeys({"score"});
        EXPECT_EQ(notifications, 1);
        EXPECT_EQ(observedKeys, (std::vector<std::string>{"score"}));

        config.setDisabledGraphLineKeys({"score", "score"});
        EXPECT_EQ(notifications, 1);
    }
}
