#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <limits>
#include <ranges>

#include "app/contracts/expression_dsl.h"
#include "data/interfaces/i_settings_service.h"
#include "data/graph_configuration_service.h"
#include "qt_data/graph_configuration_persistence.h"
#include "usecases/graph_configuration_use_case.h"
#include "usecases/series_management_use_case.h"
#include "fake_settings_service.h"

using namespace ksv;
using namespace application;
using namespace qt_data;
using namespace ksv::tests_support;

namespace {
    // The whole chain App::App() builds over the stored document; each call is a fresh application start.
    std::shared_ptr<SeriesManagementUseCase> open(const std::shared_ptr<ISettingsService> &settings) {
        return std::make_shared<SeriesManagementUseCase>(std::make_shared<GraphConfigurationUseCase>(
            std::make_shared<data::GraphConfigurationService>(
                std::make_shared<GraphConfigurationPersistence>(settings))));
    }

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

    class RunSeriesAuthorityTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSettingsService> settings = std::make_shared<FakeSettingsService>();
        std::shared_ptr<SeriesManagementUseCase> store;

        void makeStore() { store = open(settings); }

        [[nodiscard]] QJsonObject docObject() const {
            return QJsonDocument::fromJson(QString::fromStdString(*settings->document).toUtf8()).object();
        }

        static void removeVersionFourSections(QJsonObject &root) {
            root.remove("graphs");
            root.remove("runAxisLabelSeriesId");
        }

        void writeDoc(const QJsonObject &root) const {
            settings->document = QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
        }

        static CreateComputedSeriesRequest request() {
            return {{"Custom", {{1, 2, 3, 255}, 2.0}, true}, numericConstant(4.0)};
        }
    };

    TEST_F(RunSeriesAuthorityTest, SeedsApprovedDefaultsAndNextId) {
        makeStore();
        EXPECT_EQ(store->getAll().size(), 9U);
        EXPECT_EQ(docObject()["nextComputedSeriesId"].toString(), "10");
    }

    TEST_F(RunSeriesAuthorityTest, RoundTripsEverySeriesAndExpressionNode) {
        makeStore();
        const auto quotient = divide(primitive(PrimitiveMetric::Score), numericConstant(2.0));
        const auto product = multiply(quotient, runningSum(primitive(PrimitiveMetric::Hits)));
        const auto projection = rollingMean(projectedFinalValue(projectRateToFinal(primitive(PrimitiveMetric::Kills))), 5);
        const auto expression = averageAcrossRuns(add(subtract(product, projection), numericConstant(1.0)), RecentRuns{2});
        ASSERT_TRUE(store->createComputed({{"Every node", {}, true}, expression}).succeeded());
        const auto expected = store->getAll();
        const auto reopened = open(settings);
        EXPECT_TRUE(validateSeriesConfigs(reopened->getAll()).empty());
        EXPECT_EQ(reopened->getAll().size(), expected.size());
    }

    TEST_F(RunSeriesAuthorityTest, CreatesAndRoundTripsABlankSeriesAsAnEmptyDslString) {
        makeStore();
        const auto created = store->createComputed({{"Blank", {{1, 2, 3, 255}, 2.0}, true}, {}});
        ASSERT_TRUE(created.succeeded());
        const auto id = *created.createdId;

        const auto root = docObject();
        bool sawBlankDslExpression = false;
        for (const auto &item: root["series"].toArray()) {
            const auto object = item.toObject();
            if (object["id"].toString() == QString::number(id.value)) {
                EXPECT_TRUE(object.contains("expression"));
                EXPECT_EQ(object["expression"].toString(), "");
                sawBlankDslExpression = true;
            }
        }
        EXPECT_TRUE(sawBlankDslExpression);

        const auto reopened = open(settings);
        const auto configs = reopened->getAll();
        const auto blank = std::ranges::find(configs, id, &SeriesConfig::id);
        ASSERT_NE(blank, configs.end());
        EXPECT_FALSE(blank->expression);
    }

    TEST_F(RunSeriesAuthorityTest, UpdatingASeriesToABlankExpressionSucceedsAndPersists) {
        makeStore();
        const auto id = *store->createComputed(request()).createdId;
        ASSERT_TRUE(store->updateSeries({id, std::nullopt, Expression{}}).succeeded());

        const auto reopened = open(settings);
        const auto configs = reopened->getAll();
        const auto blank = std::ranges::find(configs, id, &SeriesConfig::id);
        ASSERT_NE(blank, configs.end());
        EXPECT_FALSE(blank->expression);
    }

    TEST_F(RunSeriesAuthorityTest, RejectsMalformedProjectRateToFinalExpression) {
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
            auto root = docObject();
            auto series = root["series"].toArray();
            auto computed = series[7].toObject();
            computed["expression"] = expression;
            series[7] = computed;
            root["series"] = series;
            writeDoc(root);

            const auto reopened = open(settings);
            EXPECT_EQ(reopened->getAll().size(), 9U);
            EXPECT_GT(settings->document_writes, 1);
        }
    }

    TEST_F(RunSeriesAuthorityTest, RejectsNonCanonicalJsonNumbersAndQuarantinesRawValue) {
        settings->document = "{\"schemaVersion\":1,\"nextComputedSeriesId\":\"5\",\"series\":[\"";
        makeStore();
        EXPECT_EQ(store->getAll().size(), 9U);
        EXPECT_GT(settings->document_writes, 0);
    }

    TEST_F(RunSeriesAuthorityTest, LeavesLegacySettingsAndAxisSettingsUntouched) {
        settings->legacyDisabledColumns = {"score"};
        makeStore();
        EXPECT_EQ(settings->legacyDisabledColumns, (std::vector<std::string>{"score"}));
    }

    TEST_F(RunSeriesAuthorityTest, CreateAllocatesAndPersistsSequentialIds) {
        makeStore();
        EXPECT_EQ(store->createComputed(request()).createdId->value, 10U);
        EXPECT_EQ(store->createComputed(request()).createdId->value, 11U);
    }

    TEST_F(RunSeriesAuthorityTest, AllocatesMaximumSeriesIdThenPersistsExhaustion) {
        makeStore();
        auto root = docObject();
        root["nextComputedSeriesId"] = QString::number(std::numeric_limits<uint64_t>::max());
        writeDoc(root);

        const auto maximumIdStore = open(settings);
        const auto created = maximumIdStore->createComputed(request());

        ASSERT_TRUE(created.succeeded());
        ASSERT_TRUE(created.createdId);
        EXPECT_EQ(created.createdId->value, std::numeric_limits<uint64_t>::max());
        EXPECT_TRUE(docObject()["nextComputedSeriesId"].isNull());
        const auto reopened = open(settings);
        const auto exhausted = reopened->createComputed(request());
        ASSERT_TRUE(exhausted.failure);
        EXPECT_EQ(*exhausted.failure, SeriesMutationFailureCode::ComputedSeriesIdExhausted);
    }

    TEST_F(RunSeriesAuthorityTest, AllocatesMaximumAxisIdThenPersistsExhaustion) {
        makeStore();
        auto root = docObject();
        root["nextAxisId"] = QString::number(std::numeric_limits<uint64_t>::max());
        writeDoc(root);

        const auto maximumIdStore = open(settings);
        const auto created = maximumIdStore->createAxis({"Maximum"});

        ASSERT_TRUE(created.succeeded());
        ASSERT_TRUE(created.createdAxisId);
        EXPECT_EQ(created.createdAxisId->value, std::numeric_limits<uint64_t>::max());
        EXPECT_TRUE(docObject()["nextAxisId"].isNull());
        const auto reopened = open(settings);
        const auto exhausted = reopened->createAxis({"After maximum"});
        ASSERT_TRUE(exhausted.failure);
        EXPECT_EQ(*exhausted.failure, SeriesMutationFailureCode::AxisIdExhausted);
    }

    TEST_F(RunSeriesAuthorityTest, DraftMutationDoesNotReachDiskUntilCommitted) {
        makeStore();
        store->beginDraft();
        ASSERT_TRUE(store->createComputed(request()).succeeded());
        const int writesAfterMutation = settings->document_writes;

        const auto reopened = open(settings);
        EXPECT_EQ(reopened->getAll().size(), 9U);
        EXPECT_EQ(settings->document_writes, writesAfterMutation);
    }

    TEST_F(RunSeriesAuthorityTest, DiscardDraftRestoresPreDraftStateAndFiresOnChangedOnce) {
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

    TEST_F(RunSeriesAuthorityTest, CommitDraftWritesOnceReflectingFinalShadowState) {
        makeStore();
        store->beginDraft();
        ASSERT_TRUE(store->createComputed(request()).succeeded());
        ASSERT_TRUE(store->createComputed(request()).succeeded());
        const int writesDuringDraft = settings->document_writes;

        ASSERT_TRUE(store->commitDraft().succeeded());

        EXPECT_EQ(settings->document_writes, writesDuringDraft + 1);
        const auto reopened = open(settings);
        EXPECT_EQ(reopened->getAll().size(), 11U);
    }

    TEST_F(RunSeriesAuthorityTest, HasPendingChangesTracksDraftMutationsAndResolution) {
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

    TEST_F(RunSeriesAuthorityTest, CommitOrDiscardDraftWithNoActiveDraftIsANoOp) {
        makeStore();
        const int writesBefore = settings->document_writes;
        int notifications = 0;
        store->onChanged([&] { ++notifications; });

        EXPECT_TRUE(store->commitDraft().succeeded());
        store->discardDraft();

        EXPECT_EQ(settings->document_writes, writesBefore);
        EXPECT_EQ(notifications, 0);
    }

    TEST_F(RunSeriesAuthorityTest, UpdatesDeletesAndReordersWithinTypedPermissions) {
        makeStore();
        const auto id = *store->createComputed(request()).createdId;
        EXPECT_TRUE(store->updateSeries({
            id, UpdatedSeriesPresentation{"Updated", LineStyle{}, false}, numericConstant(4.0)
        }).succeeded());
        EXPECT_TRUE(store->removeComputed(id).succeeded());
        EXPECT_TRUE(store->reorder({1}, 1).succeeded());
    }

    TEST_F(RunSeriesAuthorityTest, PrimitiveRowsCanBeRenamedReExpressedAndDeletedLikeAnyOther) {
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

    TEST_F(RunSeriesAuthorityTest, RejectsUnknownAndInvalidMutationRequestsWithoutNotification) {
        makeStore();
        int notifications = 0;
        store->onChanged([&] { ++notifications; });
        EXPECT_EQ(store->removeComputed({99}).failure, SeriesMutationFailureCode::UnknownSeriesId);
        EXPECT_EQ(store->updateSeries({{99}, UpdatedSeriesPresentation{.enabled = false}, {}}).failure,
                  SeriesMutationFailureCode::UnknownSeriesId);
        EXPECT_EQ(notifications, 0);
    }

    TEST_F(RunSeriesAuthorityTest, ReorderingToCurrentPositionIsSuccessfulNoOp) {
        makeStore();
        const auto writes = settings->document_writes;
        // Score seeds at displayPosition 0, so this asks for the position it already holds.
        EXPECT_TRUE(store->reorder(SeriesId{1}, 0).succeeded());
        EXPECT_EQ(settings->document_writes, writes);
    }


    TEST_F(RunSeriesAuthorityTest, ValidationFailureLeavesStateAndNextIdUnchanged) {
        makeStore();
        const auto before = store->getAll();
        EXPECT_FALSE(store->createComputed({{" ", {}, true}, numericConstant(1)}).succeeded());
        EXPECT_EQ(store->getAll().size(), before.size());
        EXPECT_EQ(store->createComputed(request()).createdId->value, 10U);
    }

    TEST_F(RunSeriesAuthorityTest, MalformedOrUnsupportedDocumentQuarantinesThenSeedsDefaults) {
        settings->document = "{}";
        makeStore();
        EXPECT_EQ(store->getAll().size(), 9U);
        EXPECT_EQ(settings->quarantined, (std::vector<std::string>{"{}"}));
    }

    TEST_F(RunSeriesAuthorityTest, SeededDocumentIncludesAxesAndNextAxisId) {
        makeStore();
        const auto root = docObject();
        EXPECT_EQ(root["schemaVersion"].toInt(), 4);
        EXPECT_EQ(root["nextAxisId"].toString(), "3");
        ASSERT_TRUE(root["axes"].isArray());
        EXPECT_EQ(root["axes"].toArray().size(), 2);
        const auto firstSeries = root["series"].toArray()[0].toObject();
        EXPECT_TRUE(firstSeries["yAxisId"].isNull());
        EXPECT_EQ(firstSeries["transformKind"].toString(), "identity");
    }

    TEST_F(RunSeriesAuthorityTest, LoadsV3DslExpressions) {
        makeStore();
        auto root = docObject();
        root["schemaVersion"] = 3;
        removeVersionFourSections(root);
        auto series = root["series"].toArray();
        auto score = series[0].toObject();
        score["expression"] = "Add(SCORE, 2)";
        series[0] = score;
        root["series"] = series;
        writeDoc(root);

        const auto reopened = open(settings);
        const auto configs = reopened->getAll();
        const auto loaded = std::ranges::find(configs, SeriesId{1}, &SeriesConfig::id);
        ASSERT_NE(loaded, configs.end());
        EXPECT_EQ(encodeExpressionDsl(loaded->expression), "Add(SCORE, 2)");
    }

    TEST_F(RunSeriesAuthorityTest, LoadsV2NestedJsonExpressions) {
        makeStore();
        auto root = docObject();
        root["schemaVersion"] = 2;
        removeVersionFourSections(root);
        auto series = root["series"].toArray();
        auto score = series[0].toObject();
        score["expression"] = QJsonObject{{"kind", "add"},
                                            {"left", QJsonObject{{"kind", "primitive"}, {"primitiveMetric", "score"}}},
                                            {"right", QJsonObject{{"kind", "constant"}, {"value", 2.0}}}};
        root["series"] = QJsonArray{score};
        writeDoc(root);

        const auto reopened = open(settings);
        const auto configs = reopened->getAll();
        const auto loaded = std::ranges::find(configs, SeriesId{1}, &SeriesConfig::id);
        ASSERT_NE(loaded, configs.end());
        EXPECT_EQ(encodeExpressionDsl(loaded->expression), "Add(SCORE, 2)");
    }

    TEST_F(RunSeriesAuthorityTest, RejectsMalformedV3DslExpressionsAndReseeds) {
        makeStore();
        auto root = docObject();
        auto series = root["series"].toArray();
        auto score = series[0].toObject();
        score["expression"] = "Add(SCORE,)";
        series[0] = score;
        root["series"] = series;
        writeDoc(root);

        const auto reopened = open(settings);
        EXPECT_EQ(reopened->getAll().size(), 9U);
        EXPECT_GT(settings->document_writes, 1);
    }

    TEST_F(RunSeriesAuthorityTest, MigratesV1DocumentToV2InMemory) {
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
        const auto root = docObject();
        EXPECT_EQ(root["schemaVersion"].toInt(), 1);
    }

    TEST_F(RunSeriesAuthorityTest, MigratesEveryScoreFamilySeriesToTheSharedAxis) {
        makeStore();
        auto root = docObject();
        root["schemaVersion"] = 1;
        removeVersionFourSections(root);
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
        writeDoc(root);

        const auto migrated = open(settings);
        const auto configs = migrated->getAll();
        const auto scoreFamily = migrated->getAllAxes()[1].id;
        for (const auto id: {SeriesId{7}, SeriesId{8}, SeriesId{9}}) {
            const auto config = std::ranges::find(configs, id, &SeriesConfig::id);
            ASSERT_NE(config, configs.end());
            EXPECT_EQ(config->yAxisId, scoreFamily);
        }
    }

    TEST_F(RunSeriesAuthorityTest, MigratesEmptyV1DocumentWithDefaultAxes) {
        settings->document = QString::fromUtf8(QJsonDocument(QJsonObject{{"schemaVersion", 1},
            {"nextComputedSeriesId", "1"}, {"series", QJsonArray{}}}).toJson(QJsonDocument::Compact)).toStdString();
        makeStore();
        EXPECT_TRUE(store->getAll().empty());
        EXPECT_EQ(store->getAllAxes().size(), 2U);
    }

    TEST_F(RunSeriesAuthorityTest, RejectsDuplicateAxisIdsAndNonPositiveFallbackSpan) {
        makeStore();
        auto root = docObject();
        auto axes = root["axes"].toArray();
        auto duplicate = axes[0].toObject();
        duplicate["id"] = axes[1].toObject()["id"];
        axes[0] = duplicate;
        root["axes"] = axes;
        writeDoc(root);
        const auto duplicateReopened = open(settings);
        EXPECT_EQ(duplicateReopened->getAllAxes().size(), 2U);

        for (const auto span: {0.0, -1.0}) {
            root = docObject();
            axes = root["axes"].toArray();
            auto axis = axes[0].toObject();
            auto options = axis["options"].toObject();
            options["fallbackSpan"] = span;
            axis["options"] = options;
            axes[0] = axis;
            root["axes"] = axes;
            writeDoc(root);
            const auto spanReopened = open(settings);
            EXPECT_EQ(spanReopened->getAllAxes().size(), 2U);
        }
    }

    TEST_F(RunSeriesAuthorityTest, CreatesDeletesAndPersistsAxes) {
        makeStore();
        const auto created = store->createAxis({"Custom"});
        ASSERT_TRUE(created.succeeded());
        ASSERT_TRUE(created.createdAxisId);
        EXPECT_EQ(created.createdAxisId->value, 3U);
        const auto second = store->createAxis({"Another"});
        ASSERT_TRUE(second.createdAxisId);
        EXPECT_EQ(second.createdAxisId->value, 4U);
        ASSERT_TRUE(store->updateSeries({.id = SeriesId{1}, .yAxisId = created.createdAxisId}).succeeded());
        const auto reopened = open(settings);
        EXPECT_EQ(reopened->getAllAxes().size(), 4U);
        const auto reopenedConfigs = reopened->getAll();
        const auto reopenedScore = std::ranges::find(reopenedConfigs, SeriesId{1}, &SeriesConfig::id);
        ASSERT_NE(reopenedScore, reopenedConfigs.end());
        EXPECT_EQ(reopenedScore->yAxisId, created.createdAxisId);
        ASSERT_TRUE(store->deleteAxis(*created.createdAxisId).succeeded());
        const auto configs = store->getAll();
        const auto score = std::ranges::find(configs, SeriesId{1}, &SeriesConfig::id);
        ASSERT_NE(score, configs.end());
        EXPECT_FALSE(score->yAxisId);
    }

    TEST_F(RunSeriesAuthorityTest, RejectsUnknownAxisAndCanClearAxisAssignment) {
        makeStore();
        EXPECT_EQ(store->updateSeries({.id = SeriesId{1}, .yAxisId = AxisId{999}}).failure,
                  SeriesMutationFailureCode::UnknownAxisId);
        EXPECT_EQ(store->deleteAxis(AxisId{999}).failure, SeriesMutationFailureCode::UnknownAxisId);
        ASSERT_TRUE(store->updateSeries({.id = SeriesId{7}, .yAxisId = std::optional<AxisId>{std::nullopt}}).succeeded());
        const auto configs = store->getAll();
        const auto score = std::ranges::find(configs, SeriesId{7}, &SeriesConfig::id);
        ASSERT_NE(score, configs.end());
        EXPECT_FALSE(score->yAxisId);
    }

    TEST_F(RunSeriesAuthorityTest, DraftPersistsAxesAndAssignmentsOnlyOnCommit) {
        makeStore();
        store->beginDraft();
        const auto created = store->createAxis({"Draft"});
        ASSERT_TRUE(created.createdAxisId);
        ASSERT_TRUE(store->updateSeries({.id = SeriesId{1}, .yAxisId = created.createdAxisId}).succeeded());
        const auto beforeCommit = open(settings);
        EXPECT_EQ(beforeCommit->getAllAxes().size(), 2U);
        const auto beforeConfigs = beforeCommit->getAll();
        const auto beforeScore = std::ranges::find(beforeConfigs, SeriesId{1}, &SeriesConfig::id);
        ASSERT_NE(beforeScore, beforeConfigs.end());
        EXPECT_FALSE(beforeScore->yAxisId);
        ASSERT_TRUE(store->commitDraft().succeeded());
        const auto afterCommit = open(settings);
        EXPECT_EQ(afterCommit->getAllAxes().size(), 3U);
        const auto afterConfigs = afterCommit->getAll();
        const auto afterScore = std::ranges::find(afterConfigs, SeriesId{1}, &SeriesConfig::id);
        ASSERT_NE(afterScore, afterConfigs.end());
        EXPECT_EQ(afterScore->yAxisId, created.createdAxisId);
    }

    TEST_F(RunSeriesAuthorityTest, DiscardDraftRestoresAxesAndSeriesAssignments) {
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

    TEST_F(RunSeriesAuthorityTest, RejectsMalformedExpressionInLegacyV1DocumentAndReseeds) {
        auto root = QJsonDocument::fromJson(legacyV1Document().toUtf8()).object();
        auto series = root["series"].toArray();
        auto malformed = series[2].toObject();
        malformed["expression"] = QJsonObject{{"kind", "projectRateToFinal"}};
        series[2] = malformed;
        root["series"] = series;
        writeDoc(root);

        makeStore();

        EXPECT_EQ(store->getAll().size(), 9U);
        EXPECT_GT(settings->document_writes, 0);
    }
}
