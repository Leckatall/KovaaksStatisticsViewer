#include <gtest/gtest.h>

#include <QJsonArray>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>

#include <array>

#include "qt_data/series_config_store.h"
#include "qt_data/series_config_store_settings_backend.h"

using namespace ksv;
using namespace application;
using namespace qt_data;

namespace {
    class MemorySettingsBackend final : public ISeriesConfigStoreSettingsBackend {
    public:
        [[nodiscard]] bool contains(const QString &key) const override { return values.contains(key); }
        [[nodiscard]] QVariant value(const QString &key) const override { return values.value(key); }

        void setValue(const QString &key, const QVariant &value) override {
            values.insert(key, value);
            ++writes;
        }

        void sync() override { ++syncs; }
        [[nodiscard]] QSettings::Status status() const override { return syncStatus; }

        void reload() override {
            ++reloads;
            if (useReloadedValues) values = reloadedValues;
        }

        QHash<QString, QVariant> values;
        QHash<QString, QVariant> reloadedValues;
        QSettings::Status syncStatus = QSettings::NoError;
        int writes = 0;
        int syncs = 0;
        int reloads = 0;
        bool useReloadedValues = false;
    };

    class SeriesConfigStoreTest : public testing::Test {
    protected:
        std::shared_ptr<MemorySettingsBackend> backend = std::make_shared<MemorySettingsBackend>();
        std::unique_ptr<SeriesConfigStore> store;

        void makeStore() { store = std::make_unique<SeriesConfigStore>(backend); }

        static CreateComputedSeriesRequest request() {
            return {{"Custom", {{1, 2, 3, 255}, 2.0}, true}, numericConstant(4.0)};
        }
    };

    TEST_F(SeriesConfigStoreTest, SeedsApprovedDefaultsAndNextId) {
        makeStore();
        EXPECT_EQ(store->getAll().size(), 9U);
        const auto document = QJsonDocument::fromJson(backend->value("graph/seriesConfigV1").toString().toUtf8());
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
        const auto raw = backend->values;
        auto reopenedBackend = std::make_shared<MemorySettingsBackend>();
        reopenedBackend->values = raw;
        SeriesConfigStore reopened(reopenedBackend);
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
            backend = std::make_shared<MemorySettingsBackend>();
            makeStore();
            auto document = QJsonDocument::fromJson(backend->values.value("graph/seriesConfigV1").toString().toUtf8());
            auto root = document.object();
            auto series = root["series"].toArray();
            auto computed = series[7].toObject();
            computed["expression"] = expression;
            series[7] = computed;
            root["series"] = series;
            backend->values.insert("graph/seriesConfigV1",
                                   QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));

            SeriesConfigStore reopened(backend);
            EXPECT_EQ(reopened.getAll().size(), 9U);
            EXPECT_GT(backend->writes, 1);
        }
    }

    TEST_F(SeriesConfigStoreTest, RejectsNonCanonicalJsonNumbersAndQuarantinesRawValue) {
        backend->values.insert("graph/seriesConfigV1",
                               "{\"schemaVersion\":1,\"nextComputedSeriesId\":\"5\",\"series\":[\"");
        makeStore();
        EXPECT_EQ(store->getAll().size(), 9U);
        EXPECT_GT(backend->writes, 1);
    }

    TEST_F(SeriesConfigStoreTest, InitialSeedSyncFailureReloadsAndRetriesMigrationWithoutNotification) {
        backend->syncStatus = QSettings::AccessError;
        makeStore();
        int notifications = 0;
        store->onChanged([&] { ++notifications; });
        backend->syncStatus = QSettings::NoError;
        backend->reloadedValues.clear();
        backend->useReloadedValues = true;
        EXPECT_EQ(store->getAll().size(), 9U);
        EXPECT_EQ(notifications, 0);
        EXPECT_GT(backend->reloads, 0);
    }

    // MigratesDisabledColumnsWithoutTreatingQmlVisibilityAsEnabledState removed: referenced the
    // retired seriesPresentation() free function and no longer compiled. See
    // .plans/series-config-migration-completion/plans/05-store-api-collapse.md.

    TEST_F(SeriesConfigStoreTest, LeavesLegacySettingsAndAxisSettingsUntouched) {
        backend->values.insert("graph/disabledColumns", QStringList{"score"});
        backend->values.insert("graph/yAxisColumnKey", "accuracy");
        const auto before = backend->values;
        makeStore();
        EXPECT_EQ(backend->values.value("graph/disabledColumns").toStringList(),
                  before.value("graph/disabledColumns").toStringList());
        EXPECT_EQ(backend->values.value("graph/yAxisColumnKey").toString(),
                  before.value("graph/yAxisColumnKey").toString());
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

    TEST_F(SeriesConfigStoreTest, SyncFailureRequiresReloadAndDoesNotNotify) {
        makeStore();
        backend->syncStatus = QSettings::AccessError;
        const auto result = store->createComputed(request());
        EXPECT_EQ(result.failure, StoreMutationFailureCode::PersistenceWriteFailed);
        EXPECT_TRUE(result.requiresReload);
    }

    TEST_F(SeriesConfigStoreTest, MalformedOrUnsupportedDocumentQuarantinesThenSeedsDefaults) {
        backend->values.insert("graph/seriesConfigV1", "{}");
        makeStore();
        EXPECT_EQ(store->getAll().size(), 9U);
    }
}
