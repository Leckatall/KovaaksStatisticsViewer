#include <gtest/gtest.h>

#include "data/interfaces/i_graph_line_config.h"
#include "usecases/graph_column_preferences.h"

using namespace ksv::application;

namespace {
    class FakeGraphLineConfig final : public IGraphLineConfig {
    public:
        std::vector<std::string> disabledKeys;
        int writes = 0;

        [[nodiscard]] std::vector<std::string> getDisabledGraphLineKeys() const override {
            return disabledKeys;
        }

        void setDisabledGraphLineKeys(const std::vector<std::string> &keys) override {
            disabledKeys = keys;
            ++writes;
        }

        void onDisabledGraphLinesChanged(std::function<void()>) override {}
    };

    class GraphColumnPreferencesTest : public testing::Test {
    protected:
        std::shared_ptr<FakeGraphLineConfig> config = std::make_shared<FakeGraphLineConfig>();
        GraphColumnPreferences preferences{config};
    };

    TEST_F(GraphColumnPreferencesTest, DefaultsToAllColumnsInDisplayOrder) {
        EXPECT_EQ(preferences.getEnabledColumns(),
                  (std::vector<ColumnId>(kPlottableColumnIds.begin(), kPlottableColumnIds.end())));
    }

    TEST_F(GraphColumnPreferencesTest, DisablesAndReEnablesColumn) {
        preferences.setEnabled(ColumnId::Accuracy, false);

        EXPECT_FALSE(preferences.isEnabled(ColumnId::Accuracy));
        EXPECT_EQ(config->disabledKeys, (std::vector<std::string>{"accuracy"}));

        preferences.setEnabled(ColumnId::Accuracy, true);

        EXPECT_TRUE(preferences.isEnabled(ColumnId::Accuracy));
        EXPECT_TRUE(config->disabledKeys.empty());
    }

    TEST_F(GraphColumnPreferencesTest, AllowsEveryColumnToBeDisabled) {
        for (const auto column: kPlottableColumnIds) preferences.setEnabled(column, false);

        EXPECT_TRUE(preferences.getEnabledColumns().empty());
    }

    TEST_F(GraphColumnPreferencesTest, IgnoresAndPreservesUnknownKeys) {
        config->disabledKeys = {"futureColumn", "score"};

        const auto enabled = preferences.getEnabledColumns();
        EXPECT_EQ(enabled.size(), kPlottableColumnIds.size() - 1);
        EXPECT_FALSE(preferences.isEnabled(ColumnId::Score));

        preferences.setEnabled(ColumnId::Accuracy, false);
        EXPECT_EQ(config->disabledKeys,
                  (std::vector<std::string>{"futureColumn", "score", "accuracy"}));
    }

    TEST_F(GraphColumnPreferencesTest, SemanticNoOpDoesNotWrite) {
        preferences.setEnabled(ColumnId::Score, true);
        EXPECT_EQ(config->writes, 0);

        config->disabledKeys = {"score"};
        preferences.setEnabled(ColumnId::Score, false);
        EXPECT_EQ(config->writes, 0);
    }

    TEST_F(GraphColumnPreferencesTest, RejectsNonPlottableColumn) {
        preferences.setEnabled(ColumnId::Time, false);

        EXPECT_FALSE(preferences.isEnabled(ColumnId::Time));
        EXPECT_EQ(config->writes, 0);
    }
}
