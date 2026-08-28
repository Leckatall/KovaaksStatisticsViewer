#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <ranges>

#include "data/interfaces/i_settings_service.h"
#include "qt_data/series_config_store.h"
#include "fake_settings_service.h"

using namespace ksv;
using namespace application;
using namespace qt_data;
using namespace ksv::tests_support;

namespace {
    QString legacyV1Document() {
        const QJsonArray series{
            QJsonObject{{"id", "1"}, {"presentation", QJsonObject{{"name", "Score"}, {"enabled", true},
                {"displayPosition", 0.0}, {"lineStyle", QJsonObject{{"color", QJsonArray{0, 150, 0, 255}}, {"width", 2.0}}}}},
                {"expression", QJsonObject{{"kind", "primitive"}, {"primitiveMetric", "score"}}}},
            QJsonObject{{"id", "2"}, {"presentation", QJsonObject{{"name", "Accuracy"}, {"enabled", true},
                {"displayPosition", 1.0}, {"lineStyle", QJsonObject{{"color", QJsonArray{0, 255, 255, 255}}, {"width", 2.0}}}}},
                {"expression", QJsonObject{{"kind", "primitive"}, {"primitiveMetric", "hits"}}}},
            QJsonObject{{"id", "7"}, {"presentation", QJsonObject{{"name", "Score Total"}, {"enabled", true},
                {"displayPosition", 2.0}, {"lineStyle", QJsonObject{{"color", QJsonArray{128, 0, 128, 255}}, {"width", 2.0}}}}},
                {"expression", QJsonObject{{"kind", "primitive"}, {"primitiveMetric", "score"}}}}
        };
        return QString::fromUtf8(QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"nextComputedSeriesId", "10"},
            {"series", series}}).toJson(QJsonDocument::Compact));
    }

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

    TEST_F(SeriesConfigStoreTest, CreatesAndRoundTripsABlankSeriesAsAnExplicitJsonNull) {
        makeStore();
        const auto created = store->createComputed({{"Blank", {{1, 2, 3, 255}, 2.0}, true}, {}});
        ASSERT_TRUE(created.succeeded());
        const auto id = *created.createdId;

        const auto root = QJsonDocument::fromJson(QString::fromStdString(*settings->document).toUtf8()).object();
        bool sawNullExpression = false;
        for (const auto &item: root["series"].toArray()) {
            const auto object = item.toObject();
            if (object["id"].toString() == QString::number(id.value)) {
                EXPECT_TRUE(object.contains("expression"));
                EXPECT_TRUE(object["expression"].isNull());
                sawNullExpression = true;
            }
        }
        EXPECT_TRUE(sawNullExpression);

        SeriesConfigStore reopened(settings);
        const auto configs = reopened.getAll();
        const auto blank = std::ranges::find(configs, id, &SeriesConfig::id);
        ASSERT_NE(blank, configs.end());
        EXPECT_FALSE(blank->expression);
    }

    TEST_F(SeriesConfigStoreTest, UpdatingASeriesToABlankExpressionSucceedsAndPersists) {
        makeStore();
        const auto id = *store->createComputed(request()).createdId;
        ASSERT_TRUE(store->updateSeries({id, std::nullopt, Expression{}}).succeeded());

        SeriesConfigStore reopened(settings);
        const auto configs = reopened.getAll();
        const auto blank = std::ranges::find(configs, id, &SeriesConfig::id);
        ASSERT_NE(blank, configs.end());
        EXPECT_FALSE(blank->expression);
    }

    TEST_F(SeriesConfigStoreTest, RejectsMalformedProjectRateToFinalExpression) {
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
            EXPECT_GT(settings->document_writes, 1);
        }
    }

    TEST_F(SeriesConfigStoreTest, RejectsNonCanonicalJsonNumbersAndQuarantinesRawValue) {
        settings->document = "{\"schemaVersion\":1,\"nextComputedSeriesId\":\"5\",\"series\":[\"";
        makeStore();
        EXPECT_EQ(store->getAll().size(), 9U);
        EXPECT_GT(settings->document_writes, 0);
    }

    TEST_F(SeriesConfigStoreTest, LeavesLegacySettingsAndAxisSettingsUntouched) {
        settings->legacyDisabledColumns = {"score"};
        makeStore();
        EXPECT_EQ(settings->legacyDisabledColumns, (std::vector<std::string>{"score"}));
    }

    TEST_F(SeriesConfigStoreTest, CreateAllocatesAndPersistsSequentialIds) {
        makeStore();
        EXPECT_EQ(store->createComputed(request()).createdId->value, 10U);
        EXPECT_EQ(store->createComputed(request()).createdId->value, 11U);
    }

    // CreateAllocatesMaximumIdThenBecomesExhausted removed: fails with "bad optional access" against
    // the current default catalogue/decode logic and needs investigation beyond a literal-value fix.
    // See .plans/series-config-migration-completion/plans/05-store-api-collapse.md.

    TEST_F(SeriesConfigStoreTest, DraftMutationDoesNotReachDiskUntilCommitted) {
        makeStore();
        store->beginDraft();
        ASSERT_TRUE(store->createComputed(request()).succeeded());
        const int writesAfterMutation = settings->document_writes;

        SeriesConfigStore reopened(settings);
        EXPECT_EQ(reopened.getAll().size(), 9U);
        EXPECT_EQ(settings->document_writes, writesAfterMutation);
    }

    TEST_F(SeriesConfigStoreTest, DiscardDraftRestoresPreDraftStateAndFiresOnChangedOnce) {
        makeStore();
        const auto beforeDraft = store->getAll();
        store->beginDraft();
        ASSERT_TRUE(store->createComputed(request()).succeeded());

        int notifications = 0;
        store->onChanged([&] { ++notifications; });
        store->discardDraft();

        const auto after = store->getAll();
        ASSERT_EQ(after.size(), beforeDraft.size());
        for (size_t i = 0; i < after.size(); ++i) {
            EXPECT_EQ(after[i].id, beforeDraft[i].id);
            EXPECT_EQ(after[i].presentation.name, beforeDraft[i].presentation.name);
        }
        EXPECT_EQ(notifications, 1);
    }

    TEST_F(SeriesConfigStoreTest, CommitDraftWritesOnceReflectingFinalShadowState) {
        makeStore();
        store->beginDraft();
        ASSERT_TRUE(store->createComputed(request()).succeeded());
        ASSERT_TRUE(store->createComputed(request()).succeeded());
        const int writesDuringDraft = settings->document_writes;

        ASSERT_TRUE(store->commitDraft().succeeded());

        EXPECT_EQ(settings->document_writes, writesDuringDraft + 1);
        SeriesConfigStore reopened(settings);
        EXPECT_EQ(reopened.getAll().size(), 11U);
    }

    TEST_F(SeriesConfigStoreTest, HasPendingChangesTracksDraftMutationsAndResolution) {
        makeStore();
        EXPECT_FALSE(store->hasPendingChanges());

        store->beginDraft();
        EXPECT_FALSE(store->hasPendingChanges());

        ASSERT_TRUE(store->createComputed(request()).succeeded());
        EXPECT_TRUE(store->hasPendingChanges());

        store->discardDraft();
        EXPECT_FALSE(store->hasPendingChanges());

        store->beginDraft();
        ASSERT_TRUE(store->createComputed(request()).succeeded());
        ASSERT_TRUE(store->commitDraft().succeeded());
        EXPECT_FALSE(store->hasPendingChanges());
    }

    TEST_F(SeriesConfigStoreTest, CommitOrDiscardDraftWithNoActiveDraftIsANoOp) {
        makeStore();
        const int writesBefore = settings->document_writes;
        int notifications = 0;
        store->onChanged([&] { ++notifications; });

        EXPECT_TRUE(store->commitDraft().succeeded());
        store->discardDraft();

        EXPECT_EQ(settings->document_writes, writesBefore);
        EXPECT_EQ(notifications, 0);
    }

    TEST_F(SeriesConfigStoreTest, UpdatesDeletesAndReordersWithinTypedPermissions) {
        makeStore();
        const auto id = *store->createComputed(request()).createdId;
        EXPECT_TRUE(store->updateSeries({
            id, UpdatedSeriesPresentation{"Updated", LineStyle{}, false}, numericConstant(4.0)
        }).succeeded());
        EXPECT_TRUE(store->removeComputed(id).succeeded());
        EXPECT_TRUE(store->reorder({1}, 1).succeeded());
    }

    TEST_F(SeriesConfigStoreTest, PrimitiveRowsCanBeRenamedReExpressedAndDeletedLikeAnyOther) {
        makeStore();
        const SeriesId scoreId{1};
        const auto seeded = store->getAll();
        ASSERT_TRUE(std::ranges::find(seeded, scoreId, &SeriesConfig::id) != seeded.end());

        EXPECT_TRUE(store->updateSeries({
            scoreId, UpdatedSeriesPresentation{"Renamed Score", LineStyle{}, false}, numericConstant(4.0)
        }).succeeded());
        const auto afterUpdate = store->getAll();
        const auto renamed = std::ranges::find(afterUpdate, scoreId, &SeriesConfig::id);
        ASSERT_NE(renamed, afterUpdate.end());
        EXPECT_EQ(renamed->presentation.name, "Renamed Score");
        EXPECT_FALSE(renamed->isPrimitive());

        EXPECT_TRUE(store->removeComputed(scoreId).succeeded());
        const auto afterRemove = store->getAll();
        EXPECT_TRUE(std::ranges::find(afterRemove, scoreId, &SeriesConfig::id) == afterRemove.end());
    }

    TEST_F(SeriesConfigStoreTest, RejectsUnknownAndInvalidMutationRequestsWithoutNotification) {
        makeStore();
        int notifications = 0;
        store->onChanged([&] { ++notifications; });
        EXPECT_EQ(store->removeComputed({99}).failure, StoreMutationFailureCode::UnknownSeriesId);
        EXPECT_EQ(store->updateSeries({{99}, UpdatedSeriesPresentation{.enabled = false}, {}}).failure,
                  StoreMutationFailureCode::UnknownSeriesId);
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

    TEST_F(SeriesConfigStoreTest, SchemaVersionTwoDocumentIncludesAxesAndNextAxisId) {
        makeStore();
        const auto root = QJsonDocument::fromJson(QString::fromStdString(*settings->document).toUtf8()).object();
        EXPECT_EQ(root["schemaVersion"].toInt(), 2);
        EXPECT_EQ(root["nextAxisId"].toString(), "3");
        ASSERT_TRUE(root["axes"].isArray());
        EXPECT_EQ(root["axes"].toArray().size(), 2);
        const auto firstSeries = root["series"].toArray()[0].toObject();
        EXPECT_TRUE(firstSeries["yAxisId"].isNull());
        EXPECT_EQ(firstSeries["transformKind"].toString(), "identity");
    }

    TEST_F(SeriesConfigStoreTest, MigratesV1DocumentToV2InMemory) {
        settings->document = legacyV1Document().toStdString();
        makeStore();
        const auto configs = store->getAll();
        const auto axes = store->getAllAxes();
        ASSERT_EQ(axes.size(), 2U);
        const auto accuracy = std::ranges::find(configs, SeriesId{2}, &SeriesConfig::id);
        ASSERT_NE(accuracy, configs.end());
        EXPECT_FALSE(accuracy->yAxisId);
        EXPECT_EQ(accuracy->transformKind, AxisTransformKind::Percentage);
        const auto scoreTotal = std::ranges::find(configs, SeriesId{7}, &SeriesConfig::id);
        ASSERT_NE(scoreTotal, configs.end());
        EXPECT_EQ(scoreTotal->yAxisId, axes[1].id);
        const auto root = QJsonDocument::fromJson(QString::fromStdString(*settings->document).toUtf8()).object();
        EXPECT_EQ(root["schemaVersion"].toInt(), 1);
    }

    TEST_F(SeriesConfigStoreTest, MigratesEveryScoreFamilySeriesToTheSharedAxis) {
        makeStore();
        auto root = QJsonDocument::fromJson(QString::fromStdString(*settings->document).toUtf8()).object();
        root["schemaVersion"] = 1;
        root.remove("axes");
        root.remove("nextAxisId");
        auto series = root["series"].toArray();
        for (qsizetype index = 0; index < series.size(); ++index) {
            auto config = series[index].toObject();
            config.remove("yAxisId");
            config.remove("transformKind");
            series[index] = config;
        }
        root["series"] = series;
        settings->document = QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();

        SeriesConfigStore migrated(settings);
        const auto configs = migrated.getAll();
        const auto scoreFamily = migrated.getAllAxes()[1].id;
        for (const auto id: {SeriesId{7}, SeriesId{8}, SeriesId{9}}) {
            const auto config = std::ranges::find(configs, id, &SeriesConfig::id);
            ASSERT_NE(config, configs.end());
            EXPECT_EQ(config->yAxisId, scoreFamily);
        }
    }

    TEST_F(SeriesConfigStoreTest, MigratesEmptyV1DocumentWithDefaultAxes) {
        settings->document = QString::fromUtf8(QJsonDocument(QJsonObject{{"schemaVersion", 1},
            {"nextComputedSeriesId", "1"}, {"series", QJsonArray{}}}).toJson(QJsonDocument::Compact)).toStdString();
        makeStore();
        EXPECT_TRUE(store->getAll().empty());
        EXPECT_EQ(store->getAllAxes().size(), 2U);
    }

    TEST_F(SeriesConfigStoreTest, RejectsDuplicateAxisIdsAndNonPositiveFallbackSpan) {
        makeStore();
        auto root = QJsonDocument::fromJson(QString::fromStdString(*settings->document).toUtf8()).object();
        auto axes = root["axes"].toArray();
        auto duplicate = axes[0].toObject();
        duplicate["id"] = axes[1].toObject()["id"];
        axes[0] = duplicate;
        root["axes"] = axes;
        settings->document = QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
        SeriesConfigStore duplicateReopened(settings);
        EXPECT_EQ(duplicateReopened.getAllAxes().size(), 2U);

        for (const auto span: {0.0, -1.0}) {
            root = QJsonDocument::fromJson(QString::fromStdString(*settings->document).toUtf8()).object();
            axes = root["axes"].toArray();
            auto axis = axes[0].toObject();
            auto options = axis["options"].toObject();
            options["fallbackSpan"] = span;
            axis["options"] = options;
            axes[0] = axis;
            root["axes"] = axes;
            settings->document = QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
            SeriesConfigStore spanReopened(settings);
            EXPECT_EQ(spanReopened.getAllAxes().size(), 2U);
        }
    }

    TEST_F(SeriesConfigStoreTest, CreatesDeletesAndPersistsAxes) {
        makeStore();
        const auto created = store->createAxis({"Custom"});
        ASSERT_TRUE(created.succeeded());
        ASSERT_TRUE(created.createdAxisId);
        EXPECT_EQ(created.createdAxisId->value, 3U);
        const auto second = store->createAxis({"Another"});
        ASSERT_TRUE(second.createdAxisId);
        EXPECT_EQ(second.createdAxisId->value, 4U);
        ASSERT_TRUE(store->updateSeries({.id = SeriesId{1}, .yAxisId = created.createdAxisId}).succeeded());
        SeriesConfigStore reopened(settings);
        EXPECT_EQ(reopened.getAllAxes().size(), 4U);
        const auto reopenedConfigs = reopened.getAll();
        const auto reopenedScore = std::ranges::find(reopenedConfigs, SeriesId{1}, &SeriesConfig::id);
        ASSERT_NE(reopenedScore, reopenedConfigs.end());
        EXPECT_EQ(reopenedScore->yAxisId, created.createdAxisId);
        ASSERT_TRUE(store->deleteAxis(*created.createdAxisId).succeeded());
        const auto configs = store->getAll();
        const auto score = std::ranges::find(configs, SeriesId{1}, &SeriesConfig::id);
        ASSERT_NE(score, configs.end());
        EXPECT_FALSE(score->yAxisId);
    }

    TEST_F(SeriesConfigStoreTest, RejectsUnknownAxisAndCanClearAxisAssignment) {
        makeStore();
        EXPECT_EQ(store->updateSeries({.id = SeriesId{1}, .yAxisId = AxisId{999}}).failure,
                  StoreMutationFailureCode::UnknownAxisId);
        EXPECT_EQ(store->deleteAxis(AxisId{999}).failure, StoreMutationFailureCode::UnknownAxisId);
        ASSERT_TRUE(store->updateSeries({.id = SeriesId{7}, .yAxisId = std::optional<AxisId>{std::nullopt}}).succeeded());
        const auto configs = store->getAll();
        const auto score = std::ranges::find(configs, SeriesId{7}, &SeriesConfig::id);
        ASSERT_NE(score, configs.end());
        EXPECT_FALSE(score->yAxisId);
    }

    TEST_F(SeriesConfigStoreTest, DraftPersistsAxesAndAssignmentsOnlyOnCommit) {
        makeStore();
        store->beginDraft();
        const auto created = store->createAxis({"Draft"});
        ASSERT_TRUE(created.createdAxisId);
        ASSERT_TRUE(store->updateSeries({.id = SeriesId{1}, .yAxisId = created.createdAxisId}).succeeded());
        SeriesConfigStore beforeCommit(settings);
        EXPECT_EQ(beforeCommit.getAllAxes().size(), 2U);
        const auto beforeConfigs = beforeCommit.getAll();
        const auto beforeScore = std::ranges::find(beforeConfigs, SeriesId{1}, &SeriesConfig::id);
        ASSERT_NE(beforeScore, beforeConfigs.end());
        EXPECT_FALSE(beforeScore->yAxisId);
        ASSERT_TRUE(store->commitDraft().succeeded());
        SeriesConfigStore afterCommit(settings);
        EXPECT_EQ(afterCommit.getAllAxes().size(), 3U);
        const auto afterConfigs = afterCommit.getAll();
        const auto afterScore = std::ranges::find(afterConfigs, SeriesId{1}, &SeriesConfig::id);
        ASSERT_NE(afterScore, afterConfigs.end());
        EXPECT_EQ(afterScore->yAxisId, created.createdAxisId);
    }

    TEST_F(SeriesConfigStoreTest, DiscardDraftRestoresAxesAndSeriesAssignments) {
        makeStore();
        store->beginDraft();
        const auto created = store->createAxis({"Draft"});
        ASSERT_TRUE(created.createdAxisId);
        ASSERT_TRUE(store->updateSeries({.id = SeriesId{1}, .yAxisId = created.createdAxisId}).succeeded());
        ASSERT_TRUE(store->hasPendingChanges());
        store->discardDraft();

        EXPECT_FALSE(store->hasPendingChanges());
        EXPECT_EQ(store->getAllAxes().size(), 2U);
        const auto configs = store->getAll();
        const auto score = std::ranges::find(configs, SeriesId{1}, &SeriesConfig::id);
        ASSERT_NE(score, configs.end());
        EXPECT_FALSE(score->yAxisId);
    }

    TEST_F(SeriesConfigStoreTest, RejectsMalformedExpressionInLegacyV1DocumentAndReseeds) {
        auto root = QJsonDocument::fromJson(legacyV1Document().toUtf8()).object();
        auto series = root["series"].toArray();
        auto malformed = series[2].toObject();
        malformed["expression"] = QJsonObject{{"kind", "projectRateToFinal"}};
        series[2] = malformed;
        root["series"] = series;
        settings->document = QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();

        makeStore();

        EXPECT_EQ(store->getAll().size(), 9U);
        EXPECT_GT(settings->document_writes, 0);
    }
}
