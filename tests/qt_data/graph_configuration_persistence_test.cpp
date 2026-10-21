#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSettings>
#include <QTemporaryDir>

#include <algorithm>
#include <chrono>
#include <limits>
#include <ranges>

#include "app/contracts/expression_dsl.h"
#include "qt_data/graph_configuration_persistence.h"
#include "qt_data/series_config_codec.h"
#include "qt_data/settings_service.h"
#include "fake_settings_service.h"

using namespace ksv;
using namespace application;
using namespace qt_data;
using namespace ksv::tests_support;
using namespace std::chrono;

namespace {
    QJsonObject primitiveJson(const char *metric) {
        return QJsonObject{{"kind", "primitive"}, {"primitiveMetric", metric}};
    }

    QJsonObject presentationJson(const char *name, const double position, const QJsonArray &color) {
        return QJsonObject{{"name", name}, {"enabled", true}, {"displayPosition", position},
            {"lineStyle", QJsonObject{{"color", color}, {"width", 2.0}}}};
    }

    std::string compact(const QJsonObject &root) {
        return QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
    }

    std::string v1Document() {
        const QJsonArray series{
            QJsonObject{{"id", "1"}, {"presentation", presentationJson("Score", 0, {0, 150, 0, 255})},
                {"expression", primitiveJson("score")}},
            QJsonObject{{"id", "2"}, {"presentation", presentationJson("Accuracy", 1, {0, 255, 255, 255})},
                {"expression", primitiveJson("hits")}},
            QJsonObject{{"id", "7"}, {"presentation", presentationJson("Score Total", 2, {128, 0, 128, 255})},
                {"expression", primitiveJson("score")}}
        };
        return compact(QJsonObject{{"schemaVersion", 1}, {"nextComputedSeriesId", "10"}, {"series", series}});
    }

    QJsonObject v3Series(const char *id, const char *name, const double position, const char *dsl,
                         const QJsonValue &yAxisId = QJsonValue()) {
        return QJsonObject{{"id", id}, {"presentation", presentationJson(name, position, {1, 2, 3, 255})},
            {"expression", dsl}, {"yAxisId", yAxisId}, {"transformKind", "identity"}};
    }

    QJsonObject axisJson(const char *id, const char *name) {
        return QJsonObject{{"id", id}, {"name", name}, {"options", QJsonObject{{"baseline", "hugData"},
            {"integral", false}, {"targetTicks", 10}, {"fallbackSpan", 1.0}}}, {"transformKind", "identity"}};
    }

    // A user computed series on a user axis, with both allocators exhausted: the shapes an upgrade
    // must never lose.
    QJsonObject v3Root() {
        const QJsonArray series{
            v3Series("1", "Score", 0, "SCORE"),
            v3Series("12", "Custom", 1, "Add(SCORE, 2)", "7"),
            v3Series("18446744073709551615", "Last", 2, "HITS")
        };
        const QJsonArray axes{axisJson("7", "Mine"), axisJson("18446744073709551615", "Final")};
        return QJsonObject{{"schemaVersion", 3}, {"nextComputedSeriesId", QJsonValue()}, {"series", series},
            {"axes", axes}, {"nextAxisId", QJsonValue()}};
    }

    void expectV3RootSeries(const RunSeriesDocument &document) {
        ASSERT_EQ(document.series.size(), 3U);
        EXPECT_EQ(document.series[1].id, SeriesId{12});
        EXPECT_EQ(encodeExpressionDsl(document.series[1].expression), "Add(SCORE, 2)");
        EXPECT_EQ(document.series[1].yAxisId, AxisId{7});
        EXPECT_EQ(document.series[2].id, SeriesId{std::numeric_limits<uint64_t>::max()});
        ASSERT_EQ(document.axes.size(), 2U);
        EXPECT_EQ(document.axes[0].name, "Mine");
        EXPECT_FALSE(document.nextSeriesId);
        EXPECT_FALSE(document.nextAxisId);
    }

    GraphConfiguration everyPreferenceKind() {
        auto configuration = defaultGraphConfiguration();
        auto &daily = configuration.preferences(GraphKey::DailyPlaytime);
        daily.range = RangePreference{RecentDays{30}, true};
        daily.averagingWindowDays = 7;
        daily.appearance = {ConnectionMode::Straight, true};
        daily.seriesStyles["playtime"] = SeriesStyleOverride{RgbaColor{10, 20, 30, 40}, 2.5};
        daily.axisPolicies["playtime"] = FixedBounds{-12.5, 87.25};

        auto &history = configuration.preferences(GraphKey::ScenarioHistory);
        history.range = RangePreference{LatestRuns{50}, true};
        history.appearance = {ConnectionMode::PointsOnly, false};
        history.seriesStyles["hits"] = SeriesStyleOverride{std::nullopt, 1.5};
        history.seriesStyles["misses"] = SeriesStyleOverride{RgbaColor{1, 2, 3, 255}, std::nullopt};
        history.axisPolicies["score"] = StartAtZero{};
        history.axisPolicies["counts"] = FitVisibleData{};

        auto &rank = configuration.preferences(GraphKey::BenchmarkRank);
        rank.range = RangePreference{CustomDates{year_month_day{year{2026}, month{3}, day{5}},
                                                 year_month_day{year{2026}, month{10}, day{16}}}, true};
        rank.seriesStyles["rank"] = SeriesStyleOverride{RgbaColor{255, 0, 0, 255}, 4.0};

        auto &benchmarkPlaytime = configuration.preferences(GraphKey::BenchmarkPlaytime);
        benchmarkPlaytime.range = RangePreference{CustomDates{year_month_day{year{1970}, month{1}, day{1}},
                                                              year_month_day{year{9999}, month{12}, day{31}}}, false};
        benchmarkPlaytime.averagingWindowDays = 1;

        configuration.preferences(GraphKey::Run).appearance = {ConnectionMode::Straight, true};
        configuration.runAxisLabelSeriesId = SeriesId{7};
        return configuration;
    }

    class GraphConfigurationPersistenceTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSettingsService> settings = std::make_shared<FakeSettingsService>();

        [[nodiscard]] GraphConfiguration load() const { return GraphConfigurationPersistence(settings).load(); }

        void save(const GraphConfiguration &configuration) const {
            GraphConfigurationPersistence(settings).save(configuration);
        }

        [[nodiscard]] QJsonObject docObject() const {
            return QJsonDocument::fromJson(QByteArray::fromStdString(*settings->document)).object();
        }

        [[nodiscard]] QJsonObject graphSection(const char *key) const {
            return docObject()["graphs"].toObject()[key].toObject();
        }

        void writeDoc(const QJsonObject &root) const { settings->document = compact(root); }

        [[nodiscard]] QJsonObject savedRoot(const GraphConfiguration &configuration) const {
            save(configuration);
            return docObject();
        }

        [[nodiscard]] QJsonObject v4RootWithV3Series() const {
            auto root = savedRoot(everyPreferenceKind());
            for (const auto *key: {"series", "axes", "nextComputedSeriesId", "nextAxisId"}) root[key] = v3Root()[key];
            root["schemaVersion"] = 4;
            return root;
        }

        [[nodiscard]] static QJsonObject withPresentationKey(QJsonObject root, const QJsonValue &value) {
            auto series = root["series"].toArray();
            auto first = series[0].toObject();
            auto presentation = first["presentation"].toObject();
            if (value.isUndefined()) presentation.remove("hiddenLeadingPoints");
            else presentation["hiddenLeadingPoints"] = value;
            first["presentation"] = presentation;
            series[0] = first;
            root["series"] = series;
            return root;
        }
    };

    TEST_F(GraphConfigurationPersistenceTest, FreshInstallSeedsExactlyTheDefaults) {
        EXPECT_EQ(load(), defaultGraphConfiguration());
        EXPECT_EQ(load().runSeries, defaultRunSeriesDocument());
    }

    TEST_F(GraphConfigurationPersistenceTest, LegacyDisabledColumnKeysMapOntoTheNewLinesById) {
        settings->legacyDisabledColumns = {"scoreTotal", "accuracy", "shots"};
        const auto loaded = load();

        auto expected = defaultGraphConfiguration();
        expected.runSeries.series[0].presentation.enabled = false;
        expected.runSeries.series[4].presentation.enabled = false;
        EXPECT_EQ(loaded, expected);
    }

    TEST_F(GraphConfigurationPersistenceTest, LegacyColumnKeysWithoutACounterpartAreIgnored) {
        settings->legacyDisabledColumns = {"score", "shots", "kills", "dmg"};
        EXPECT_EQ(load(), defaultGraphConfiguration());
    }

    TEST_F(GraphConfigurationPersistenceTest, SeedsCurrentDefaultsAndWritesAVersionFiveDocument) {
        const auto loaded = load();

        EXPECT_EQ(loaded, defaultGraphConfiguration());
        EXPECT_TRUE(settings->quarantined.empty());
        ASSERT_EQ(settings->document_writes, 1);
        const auto root = docObject();
        EXPECT_EQ(root["schemaVersion"].toInt(), 5);
        EXPECT_EQ(root["graphs"].toObject().keys(),
                  (QStringList{"benchmarkPlaytime", "benchmarkRank", "dailyPlaytime", "run", "scenarioHistory"}));
    }

    TEST_F(GraphConfigurationPersistenceTest, MigratesVersionOneSeriesAndAddsDefaultGraphPreferences) {
        settings->document = v1Document();
        const auto loaded = load();

        EXPECT_TRUE(settings->quarantined.empty());
        EXPECT_EQ(settings->document_writes, 0);
        ASSERT_EQ(loaded.runSeries.series.size(), 3U);
        ASSERT_EQ(loaded.runSeries.axes.size(), 2U);
        EXPECT_EQ(loaded.runSeries.axes[0].id, AxisId{1});
        EXPECT_EQ(loaded.runSeries.axes[0].name, "Accuracy");
        EXPECT_EQ(loaded.runSeries.axes[0].transformKind, AxisTransformKind::Percentage);
        EXPECT_EQ(loaded.runSeries.axes[1].id, AxisId{2});
        EXPECT_EQ(loaded.runSeries.axes[1].name, "Score Family");
        EXPECT_EQ(loaded.runSeries.axes[1].transformKind, AxisTransformKind::Identity);
        EXPECT_EQ(loaded.runSeries.series[1].transformKind, AxisTransformKind::Percentage);
        EXPECT_EQ(loaded.runSeries.series[2].yAxisId, AxisId{2});
        EXPECT_EQ(loaded.runSeries.nextSeriesId, SeriesId{10});
        EXPECT_EQ(loaded.runSeries.nextAxisId, AxisId{3});
        for (const auto key: kGraphKeys) EXPECT_EQ(loaded.preferences(key), defaultGraphPreferences(key));

        save(loaded);
        EXPECT_EQ(docObject()["schemaVersion"].toInt(), 5);
        EXPECT_EQ(load(), loaded);
    }

    TEST_F(GraphConfigurationPersistenceTest, MigratesVersionTwoNestedExpressions) {
        auto root = v3Root();
        root["schemaVersion"] = 2;
        auto series = root["series"].toArray();
        auto custom = series[1].toObject();
        custom["expression"] = QJsonObject{{"kind", "add"}, {"left", primitiveJson("score")},
            {"right", QJsonObject{{"kind", "constant"}, {"value", 2.0}}}};
        series[1] = custom;
        auto first = series[0].toObject();
        first["expression"] = primitiveJson("score");
        series[0] = first;
        auto last = series[2].toObject();
        last["expression"] = primitiveJson("hits");
        series[2] = last;
        root["series"] = series;
        writeDoc(root);

        const auto loaded = load();
        EXPECT_TRUE(settings->quarantined.empty());
        expectV3RootSeries(loaded.runSeries);
        for (const auto key: kGraphKeys) EXPECT_EQ(loaded.preferences(key), defaultGraphPreferences(key));
        save(loaded);
        EXPECT_EQ(docObject()["series"].toArray()[1].toObject()["expression"].toString(), "Add(SCORE, 2)");
        EXPECT_EQ(load(), loaded);
    }

    TEST_F(GraphConfigurationPersistenceTest, VersionThreeSeriesAxesAndExhaustedAllocatorsSurviveTheUpgrade) {
        writeDoc(v3Root());
        const auto loaded = load();
        EXPECT_TRUE(settings->quarantined.empty());
        expectV3RootSeries(loaded.runSeries);

        const auto root = savedRoot(loaded);
        EXPECT_EQ(root["schemaVersion"].toInt(), 5);
        EXPECT_TRUE(root["nextComputedSeriesId"].isNull());
        EXPECT_TRUE(root["nextAxisId"].isNull());
        auto expectedSeries = v3Root()["series"].toArray();
        for (qsizetype index = 0; index < expectedSeries.size(); ++index) {
            auto series = expectedSeries[index].toObject();
            auto presentation = series["presentation"].toObject();
            presentation["hiddenLeadingPoints"] = 0;
            series["presentation"] = presentation;
            expectedSeries[index] = series;
        }
        EXPECT_EQ(root["series"], expectedSeries);
        EXPECT_EQ(root["axes"], v3Root()["axes"]);
        expectV3RootSeries(load().runSeries);
        for (const auto &config: load().runSeries.series) EXPECT_EQ(config.presentation.hiddenLeadingPoints, 0U);
    }

    TEST_F(GraphConfigurationPersistenceTest, HiddenLeadingPointsRoundTripAndAreAlwaysWritten) {
        auto configuration = everyPreferenceKind();
        configuration.runSeries.series[2].presentation.hiddenLeadingPoints = 3;
        save(configuration);

        const auto root = docObject();
        EXPECT_EQ(root["schemaVersion"].toInt(), 5);
        for (const auto &item: root["series"].toArray())
            EXPECT_TRUE(item.toObject()["presentation"].toObject()["hiddenLeadingPoints"].isDouble());
        EXPECT_EQ(root["series"].toArray()[2].toObject()["presentation"].toObject()["hiddenLeadingPoints"].toInt(), 3);
        EXPECT_EQ(load(), configuration);
        EXPECT_TRUE(settings->quarantined.empty());
    }

    TEST_F(GraphConfigurationPersistenceTest, VersionFourSeriesWithoutTheKeyDecodeToZeroHiddenPoints) {
        writeDoc(v4RootWithV3Series());
        const auto loaded = load();
        EXPECT_TRUE(settings->quarantined.empty());
        expectV3RootSeries(loaded.runSeries);
        for (const auto &config: loaded.runSeries.series) EXPECT_EQ(config.presentation.hiddenLeadingPoints, 0U);
    }

    // Older builds use exactKeys, so the version bump is what keeps the key out of v4 documents.
    TEST_F(GraphConfigurationPersistenceTest, RejectsHiddenLeadingPointsKeyInVersionFourAndItsAbsenceOrExcessInFive) {
        const auto rejected = [&](const QJsonObject &root) {
            settings = std::make_shared<FakeSettingsService>();
            writeDoc(root);
            (void) load();
            return settings->quarantined.size() == 1U;
        };
        EXPECT_TRUE(rejected(withPresentationKey(v4RootWithV3Series(), 2)));
        EXPECT_TRUE(rejected(withPresentationKey(savedRoot(everyPreferenceKind()), QJsonValue::Undefined)));
        EXPECT_TRUE(rejected(withPresentationKey(savedRoot(everyPreferenceKind()),
                                                 static_cast<int>(kMaximumHiddenLeadingPoints) + 1)));
    }

    TEST_F(GraphConfigurationPersistenceTest, RoundTripsEveryPreferenceKind) {
        const auto configuration = everyPreferenceKind();
        save(configuration);
        EXPECT_EQ(settings->document_writes, 1);
        EXPECT_EQ(load(), configuration);
        EXPECT_TRUE(settings->quarantined.empty());
    }

    TEST_F(GraphConfigurationPersistenceTest, StoresCustomDatesAsIsoCalendarDates) {
        const auto root = savedRoot(everyPreferenceKind());
        const auto rankRange = root["graphs"].toObject()["benchmarkRank"].toObject()["range"].toObject();
        EXPECT_EQ(rankRange["kind"].toString(), "customDates");
        EXPECT_EQ(rankRange["start"].toString(), "2026-03-05");
        EXPECT_EQ(rankRange["end"].toString(), "2026-10-16");
        const auto playtimeRange = root["graphs"].toObject()["benchmarkPlaytime"].toObject()["range"].toObject();
        EXPECT_EQ(playtimeRange["start"].toString(), "1970-01-01");
        EXPECT_EQ(playtimeRange["end"].toString(), "9999-12-31");
    }

    // QUAL-02: the stored document must not depend on the locale the app ran under when it was written.
    TEST_F(GraphConfigurationPersistenceTest, WritesLocaleIndependentNumbers) {
        const QLocale previous;
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
        const auto configuration = everyPreferenceKind();
        save(configuration);
        QLocale::setDefault(previous);

        EXPECT_NE(settings->document->find("87.25"), std::string::npos);
        EXPECT_EQ(settings->document->find("87,25"), std::string::npos);
        const auto policy = graphSection("dailyPlaytime")["axisPolicies"].toObject()["playtime"].toObject();
        EXPECT_EQ(policy["mode"].toString(), "fixed");
        EXPECT_EQ(policy["min"].toDouble(), -12.5);
        EXPECT_EQ(policy["max"].toDouble(), 87.25);
        EXPECT_EQ(load(), configuration);
    }

    TEST_F(GraphConfigurationPersistenceTest, ImportsTheLegacyRunAxisLabelWhenTheDocumentHasNone) {
        settings->legacyRunAxisLabelSeriesId = "7";
        writeDoc(v3Root());
        EXPECT_EQ(load().runAxisLabelSeriesId, SeriesId{7});

        settings->document.reset();
        settings->document_writes = 0;
        EXPECT_EQ(load().runAxisLabelSeriesId, SeriesId{7});
        EXPECT_EQ(docObject()["runAxisLabelSeriesId"].toString(), "7");
    }

    TEST_F(GraphConfigurationPersistenceTest, PrefersTheDocumentsRunAxisLabelOverTheLegacySetting) {
        auto configuration = defaultGraphConfiguration();
        configuration.runAxisLabelSeriesId = SeriesId{2};
        save(configuration);
        settings->legacyRunAxisLabelSeriesId = "7";
        EXPECT_EQ(load().runAxisLabelSeriesId, SeriesId{2});

        // An explicit "no selection" in a version-four document is also the document's own choice.
        configuration.runAxisLabelSeriesId.reset();
        save(configuration);
        EXPECT_TRUE(docObject()["runAxisLabelSeriesId"].isNull());
        EXPECT_FALSE(load().runAxisLabelSeriesId);
    }

    TEST_F(GraphConfigurationPersistenceTest, IgnoresAnEmptyOrMalformedLegacyRunAxisLabel) {
        for (const auto *legacy: {"", "abc", "0", "07", "-1"}) {
            settings->legacyRunAxisLabelSeriesId = legacy;
            writeDoc(v3Root());
            EXPECT_FALSE(load().runAxisLabelSeriesId) << legacy;
            EXPECT_TRUE(settings->quarantined.empty()) << legacy;
        }
    }

    TEST_F(GraphConfigurationPersistenceTest, MissingGraphSectionsTakeDefaultsWithoutQuarantine) {
        auto root = savedRoot(everyPreferenceKind());
        auto graphs = root["graphs"].toObject();
        graphs.remove("dailyPlaytime");
        root["graphs"] = graphs;
        writeDoc(root);

        const auto loaded = load();
        EXPECT_TRUE(settings->quarantined.empty());
        EXPECT_EQ(loaded.preferences(GraphKey::DailyPlaytime), defaultGraphPreferences(GraphKey::DailyPlaytime));
        EXPECT_EQ(loaded.preferences(GraphKey::BenchmarkRank),
                  everyPreferenceKind().preferences(GraphKey::BenchmarkRank));

        root.remove("graphs");
        writeDoc(root);
        const auto withoutGraphs = load();
        EXPECT_TRUE(settings->quarantined.empty());
        for (const auto key: kGraphKeys)
            EXPECT_EQ(withoutGraphs.preferences(key), defaultGraphPreferences(key));
        EXPECT_EQ(withoutGraphs.runAxisLabelSeriesId, SeriesId{7});
    }

    TEST_F(GraphConfigurationPersistenceTest, QuarantinesAnInvalidGraphSectionAndKeepsSeriesAxesAndOtherGraphs) {
        const auto configuration = everyPreferenceKind();
        auto root = v4RootWithV3Series();
        auto graphs = root["graphs"].toObject();
        auto daily = graphs["dailyPlaytime"].toObject();
        daily["averagingWindowDays"] = 0;
        graphs["dailyPlaytime"] = daily;
        root["graphs"] = graphs;
        writeDoc(root);
        const auto raw = *settings->document;
        settings->document_writes = 0;

        const auto loaded = load();
        EXPECT_EQ(settings->quarantined, (std::vector<std::string>{raw}));
        EXPECT_EQ(loaded.preferences(GraphKey::DailyPlaytime), defaultGraphPreferences(GraphKey::DailyPlaytime));
        for (const auto key: {GraphKey::Run, GraphKey::ScenarioHistory, GraphKey::BenchmarkRank,
                              GraphKey::BenchmarkPlaytime})
            EXPECT_EQ(loaded.preferences(key), configuration.preferences(key)) << graphKeyName(key);
        expectV3RootSeries(loaded.runSeries);
        EXPECT_EQ(loaded.runAxisLabelSeriesId, SeriesId{7});

        // The recovered document is written back so the next start does not quarantine it again.
        EXPECT_EQ(settings->document_writes, 1);
        EXPECT_EQ(load(), loaded);
        EXPECT_EQ(settings->quarantined.size(), 1U);
    }

    TEST_F(GraphConfigurationPersistenceTest, QuarantinesStructurallyMalformedGraphSections) {
        const auto base = v4RootWithV3Series();
        const auto mutated = [&](const char *graph, const char *field, const QJsonValue &value) {
            auto root = base;
            auto graphs = root["graphs"].toObject();
            auto section = graphs[graph].toObject();
            section[field] = value;
            graphs[graph] = section;
            root["graphs"] = graphs;
            return root;
        };
        const std::vector<std::pair<const char *, QJsonObject>> cases{
            {"dailyPlaytime", mutated("dailyPlaytime", "averagingWindowDays", 3.5)},
            {"dailyPlaytime", mutated("dailyPlaytime", "averagingWindowDays", "3")},
            {"dailyPlaytime", mutated("dailyPlaytime", "range", QJsonObject{{"kind", "recentDays"}, {"days", -2}})},
            {"dailyPlaytime", mutated("dailyPlaytime", "range", QJsonObject{{"kind", "latestRuns"}, {"count", 5}})},
            {"dailyPlaytime", mutated("dailyPlaytime", "range", QJsonObject{{"kind", "lastWeek"}})},
            {"benchmarkRank", mutated("benchmarkRank", "range",
                QJsonObject{{"kind", "customDates"}, {"start", "2026-3-5"}, {"end", "2026-10-16"}})},
            {"benchmarkRank", mutated("benchmarkRank", "range",
                QJsonObject{{"kind", "customDates"}, {"start", "2026-02-30"}, {"end", "2026-10-16"}})},
            {"benchmarkRank", mutated("benchmarkRank", "range",
                QJsonObject{{"kind", "customDates"}, {"start", "05.03.2026"}, {"end", "2026-10-16"}})},
            {"benchmarkRank", mutated("benchmarkRank", "range",
                QJsonObject{{"kind", "customDates"}, {"start", "2026-10-17"}, {"end", "2026-10-16"}})},
            {"scenarioHistory", mutated("scenarioHistory", "connection", "curvy")},
            {"scenarioHistory", mutated("scenarioHistory", "axisPolicies",
                QJsonObject{{"score", QJsonObject{{"mode", "fixed"}, {"min", 5}, {"max", 5}}},
                    {"accuracy", QJsonObject{{"mode", "fitVisible"}}}, {"counts", QJsonObject{{"mode", "startAtZero"}}}})},
            {"scenarioHistory", mutated("scenarioHistory", "seriesStyles",
                QJsonObject{{"kills", QJsonObject{{"width", 2.0}}}})},
            {"scenarioHistory", mutated("scenarioHistory", "seriesStyles",
                QJsonObject{{"hits", QJsonObject{{"width", 40.0}}}})},
            {"scenarioHistory", mutated("scenarioHistory", "unexpected", true)},
            {"run", mutated("run", "range", QJsonObject{{"kind", "all"}})},
        };
        for (const auto &[graph, root]: cases) {
            settings = std::make_shared<FakeSettingsService>();
            writeDoc(root);
            const auto loaded = load();
            const auto key = *graphKeyFromName(graph);
            EXPECT_EQ(settings->quarantined.size(), 1U) << compact(root);
            EXPECT_EQ(loaded.preferences(key), defaultGraphPreferences(key)) << compact(root);
            expectV3RootSeries(GraphConfigurationPersistence(settings).load().runSeries);
        }
    }

    TEST_F(GraphConfigurationPersistenceTest, QuarantinesUnknownGraphKeysAndANonObjectGraphsSection) {
        auto root = savedRoot(everyPreferenceKind());
        auto graphs = root["graphs"].toObject();
        graphs["sessionTimeline"] = QJsonObject{};
        root["graphs"] = graphs;
        writeDoc(root);
        auto loaded = load();
        EXPECT_EQ(settings->quarantined.size(), 1U);
        EXPECT_EQ(loaded.preferences(GraphKey::BenchmarkRank), everyPreferenceKind().preferences(GraphKey::BenchmarkRank));

        settings = std::make_shared<FakeSettingsService>();
        root["graphs"] = QJsonArray{};
        writeDoc(root);
        loaded = load();
        EXPECT_EQ(settings->quarantined.size(), 1U);
        for (const auto key: kGraphKeys) EXPECT_EQ(loaded.preferences(key), defaultGraphPreferences(key));
        EXPECT_EQ(loaded.runAxisLabelSeriesId, SeriesId{7});
    }

    TEST_F(GraphConfigurationPersistenceTest, AnInvalidSeriesSectionReseedsSeriesButKeepsValidGraphPreferences) {
        settings->legacyDisabledColumns = {"accuracy"};
        auto root = savedRoot(everyPreferenceKind());
        auto series = root["series"].toArray();
        auto first = series[0].toObject();
        first["expression"] = "Add(SCORE,)";
        series[0] = first;
        root["series"] = series;
        writeDoc(root);

        const auto loaded = load();
        EXPECT_EQ(settings->quarantined.size(), 1U);
        EXPECT_TRUE(sameSeriesConfigs(loaded.runSeries.series,
                                      series_codec::seededRunSeries({"accuracy"}).series));
        for (const auto key: kGraphKeys)
            EXPECT_EQ(loaded.preferences(key), everyPreferenceKind().preferences(key)) << graphKeyName(key);
    }

    TEST_F(GraphConfigurationPersistenceTest, UndecodableOrUnsupportedDocumentsFollowTheExistingRecoveryPath) {
        for (const std::string raw: {std::string("{\"schemaVersion\":4,"), std::string("[]"), std::string("{}"),
                                     std::string("{\"schemaVersion\":6}"), std::string("{\"schemaVersion\":\"4\"}")}) {
            settings = std::make_shared<FakeSettingsService>();
            settings->legacyRunAxisLabelSeriesId = "3";
            settings->document = raw;
            const auto loaded = load();
            EXPECT_EQ(settings->quarantined, (std::vector<std::string>{raw}));
            auto expected = defaultGraphConfiguration();
            expected.runAxisLabelSeriesId = SeriesId{3};
            EXPECT_EQ(loaded, expected) << raw;
            EXPECT_EQ(docObject()["schemaVersion"].toInt(), 5);
        }
    }

    // Uses SettingsService(QSettings::IniFormat) + QSettings::setPath so the round trip goes through a
    // real QSettings instance without touching the registry.
    class GraphConfigurationQSettingsTest : public testing::Test {
    protected:
        QTemporaryDir temp_dir;

        void SetUp() override {
            ASSERT_TRUE(temp_dir.isValid());
            QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp_dir.path());
        }
    };

    TEST_F(GraphConfigurationQSettingsTest, LoadsAndSavesTheVersionFiveSnapshotAcrossRestarts) {
        {
            QSettings raw(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
            raw.setValue("graph/seriesConfigV1", QString::fromStdString(compact(v3Root())));
            raw.setValue("graphAxis/seriesId", "12");
        }

        auto configuration = everyPreferenceKind();
        {
            const auto settings = std::make_shared<SettingsService>(QSettings::IniFormat);
            GraphConfigurationPersistence persistence(settings);
            const auto upgraded = persistence.load();
            expectV3RootSeries(upgraded.runSeries);
            EXPECT_EQ(upgraded.runAxisLabelSeriesId, SeriesId{12});
            configuration.runSeries = upgraded.runSeries;
            persistence.save(configuration);
        }

        const auto settings = std::make_shared<SettingsService>(QSettings::IniFormat);
        EXPECT_EQ(GraphConfigurationPersistence(settings).load(), configuration);
        const QSettings raw(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
        const auto stored = QJsonDocument::fromJson(raw.value("graph/seriesConfigV1").toString().toUtf8()).object();
        EXPECT_EQ(stored["schemaVersion"].toInt(), 5);
        EXPECT_EQ(raw.value("graphAxis/seriesId").toString(), "12");
    }
}
