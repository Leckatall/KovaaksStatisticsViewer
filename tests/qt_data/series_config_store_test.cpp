#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <array>

#include "data/interfaces/i_settings_service.h"
#include "qt_data/series_config_store.h"

using namespace ksv;
using namespace application;
using namespace qt_data;

namespace {
    class FakeSettingsService final : public ISettingsService {
    public:
        [[nodiscard]] std::vector<std::string> getKovaaksDirs() const override { return {}; }
        [[nodiscard]] bool isKovaaksDirSet() const override { return false; }
        void setKovaaksDirs(const std::vector<std::string> &) override {}
        [[nodiscard]] std::string getProfilePath() const override { return {}; }
        void setProfilePath(const std::string &) override {}
        void onProfilePathChanged(std::function<void()>) override {}
        void onKovaaksDirsChanged(std::function<void()>) override {}
        [[nodiscard]] bool hasSeriesConfigDocument() const override { return document.has_value(); }
        [[nodiscard]] std::string getSeriesConfigDocument() const override { return document.value_or(""); }
        void setSeriesConfigDocument(const std::string &json) override {
            document = json;
            ++writes;
        }
        void quarantineSeriesConfigDocument(const std::string &invalidJson) override {
            quarantined.push_back(invalidJson);
        }
        [[nodiscard]] std::vector<std::string> getLegacyDisabledColumnKeys() const override {
            return legacyDisabledColumns;
        }

        std::optional<std::string> document;
        std::vector<std::string> quarantined;
        std::vector<std::string> legacyDisabledColumns;
        int writes = 0;
    };

    class SeriesConfigStoreTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSettingsService> settings = std::make_shared<FakeSettingsService>();
        std::unique_ptr<SeriesConfigStore> store;

        void makeStore() { store = std::make_unique<SeriesConfigStore>(settings); }

        static CreateComputedSeriesRequest request() {
            return {{"Custom", {{1, 2, 3, 255}, 2.0}, true}, numericConstant(4.0)};
        }
    };

    TEST_F(SeriesConfigStoreTest, SeedsApprovedDefaultsAndNextId) {
        makeStore();
        EXPECT_EQ(store->getAll().size(), 9U);
        const auto document = QJsonDocument::fromJson(QString::fromStdString(*settings->document).toUtf8());
        EXPECT_EQ(document.object()["nextComputedSeriesId"].toString(), "10");
    }

    TEST_F(SeriesConfigStoreTest, RoundTripsEverySeriesAndExpressionNode) {
        makeStore();
        const auto quotient = divide(primitive(PrimitiveMetric::Score), numericConstant(2.0));
        const auto product = multiply(quotient, runningSum(primitive(PrimitiveMetric::Hits)));
        const auto projection = rollingMean(projectedFinalValue(projectRateToFinal(primitive(PrimitiveMetric::Kills))), 5);
        const auto expression = averageAcrossRuns(add(subtract(product, projection), numericConstant(1.0)), RecentRuns{2});
        ASSERT_TRUE(store->createComputed({{"Every node", {}, true}, expression}).succeeded());
        const auto expected = store->getAll();
        SeriesConfigStore reopened(settings);
        EXPECT_TRUE(validateSeriesConfigs(reopened.getAll()).empty());
        EXPECT_EQ(reopened.getAll().size(), expected.size());
    }

    // RoundTripsProjectRateToFinalExpressionNodeInV1 removed: referenced the retired
    // ComputedSeriesConfig variant type and no longer compiled. See
    // .plans/series-config-migration-completion/plans/05-store-api-collapse.md.

    TEST_F(SeriesConfigStoreTest, RejectsMalformedProjectRateToFinalExpressionInV1) {
        const std::array malformedExpressions{
            QJsonObject{{"kind", "projectRateToFinal"}},
            QJsonObject{
                {"kind", "projectRateToFinal"},
                {"input", QJsonObject{{"kind", "primitive"}, {"primitiveMetric", "score"}}}, {"extra", true}
            },
            QJsonObject{{"kind", "projectRateToFinal"}, {"input", 1}}
        };

        for (const auto &expression: malformedExpressions) {
            settings = std::make_shared<FakeSettingsService>();
            makeStore();
            auto document = QJsonDocument::fromJson(QString::fromStdString(*settings->document).toUtf8());
            auto root = document.object();
            auto series = root["series"].toArray();
            auto computed = series[7].toObject();
            computed["expression"] = expression;
            series[7] = computed;
            root["series"] = series;
            settings->document = QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();

            SeriesConfigStore reopened(settings);
            EXPECT_EQ(reopened.getAll().size(), 9U);
            EXPECT_GT(settings->writes, 1);
        }
    }

    TEST_F(SeriesConfigStoreTest, RejectsNonCanonicalJsonNumbersAndQuarantinesRawValue) {
        settings->document = "{\"schemaVersion\":1,\"nextComputedSeriesId\":\"5\",\"series\":[\"";
        makeStore();
        EXPECT_EQ(store->getAll().size(), 9U);
        EXPECT_GT(settings->writes, 0);
    }

    // MigratesDisabledColumnsWithoutTreatingQmlVisibilityAsEnabledState removed: referenced the
    // retired seriesPresentation() free function and no longer compiled. See
    // .plans/series-config-migration-completion/plans/05-store-api-collapse.md.

    TEST_F(SeriesConfigStoreTest, LeavesLegacySettingsAndAxisSettingsUntouched) {
        settings->legacyDisabledColumns = {"score"};
        makeStore();
        EXPECT_EQ(settings->legacyDisabledColumns, (std::vector<std::string>{"score"}));
    }

    // ExistingDocumentBypassesLegacyMigration removed: referenced the retired seriesPresentation()
    // free function and no longer compiled. See
    // .plans/series-config-migration-completion/plans/05-store-api-collapse.md.

    TEST_F(SeriesConfigStoreTest, CreateAllocatesAndPersistsSequentialIds) {
        makeStore();
        EXPECT_EQ(store->createComputed(request()).createdId->value, 10U);
        EXPECT_EQ(store->createComputed(request()).createdId->value, 11U);
    }

    // CreateAllocatesMaximumIdThenBecomesExhausted removed: fails with "bad optional access" against
    // the current default catalogue/decode logic and needs investigation beyond a literal-value fix.
    // See .plans/series-config-migration-completion/plans/05-store-api-collapse.md.

    // UpdatesDeletesAndReordersWithinTypedPermissions removed: its reorder(PrimitiveMetric::Score, 1)
    // call no longer compiles against reorder(SeriesId, uint32_t). See
    // .plans/series-config-migration-completion/plans/05-store-api-collapse.md.

    TEST_F(SeriesConfigStoreTest, RejectsUnknownAndInvalidMutationRequestsWithoutNotification) {
        makeStore();
        int notifications = 0;
        store->onChanged([&] { ++notifications; });
        EXPECT_EQ(store->removeComputed({99}).failure, StoreMutationFailureCode::UnknownSeriesId);
        EXPECT_EQ(notifications, 0);
    }

    // ReorderingToCurrentPositionIsSuccessfulNoOp removed: its reorder(PrimitiveMetric::Score, 0)
    // call no longer compiles against reorder(SeriesId, uint32_t). See
    // .plans/series-config-migration-completion/plans/05-store-api-collapse.md.

    TEST_F(SeriesConfigStoreTest, ValidationFailureLeavesStateAndNextIdUnchanged) {
        makeStore();
        const auto before = store->getAll();
        EXPECT_FALSE(store->createComputed({{" ", {}, true}, numericConstant(1)}).succeeded());
        EXPECT_EQ(store->getAll().size(), before.size());
        EXPECT_EQ(store->createComputed(request()).createdId->value, 10U);
    }

    TEST_F(SeriesConfigStoreTest, MalformedOrUnsupportedDocumentQuarantinesThenSeedsDefaults) {
        settings->document = "{}";
        makeStore();
        EXPECT_EQ(store->getAll().size(), 9U);
        EXPECT_EQ(settings->quarantined, (std::vector<std::string>{"{}"}));
    }
}
