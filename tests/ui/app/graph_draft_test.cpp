//
// SettingsViewModel's per-graph preference draft: the controls each graph offers, locale-aware reading of
// typed numbers and dates, live issues, scoped reset and the draft's isolation from committed state.
//

#include <gtest/gtest.h>

#include <chrono>
#include <variant>

#include <QDate>
#include <QSignalSpy>

#include "presentation/app/settings_vm.h"
#include "fake_file_service.h"
#include "fake_graph_configuration_use_case.h"
#include "fake_profile_service.h"
#include "fake_settings_service.h"

using namespace ksv::presentation;
using namespace ksv::application;
using namespace ksv::tests_support;

namespace {
    class GraphDraftTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSettingsService> settings = [] {
            auto service = std::make_shared<FakeSettingsService>();
            service->dirs = {"C:/Kovaaks"};
            service->profile_path = "C:/Profile/profile.pb";
            return service;
        }();
        std::shared_ptr<FakeFileService> files = std::make_shared<FakeFileService>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        std::shared_ptr<FakeGraphConfigurationUseCase> configuration =
            std::make_shared<FakeGraphConfigurationUseCase>();

        static const QLocale &english() {
            static const QLocale locale(QLocale::English, QLocale::UnitedStates);
            return locale;
        }

        static const QLocale &german() {
            static const QLocale locale(QLocale::German, QLocale::Germany);
            return locale;
        }

        std::unique_ptr<SettingsViewModel> make(const QLocale &locale = english()) {
            auto view_model = std::make_unique<SettingsViewModel>(settings, files, profile, configuration);
            view_model->setLocale(locale);
            return view_model;
        }

        std::unique_ptr<SettingsViewModel> visit(const QLocale &locale = english()) {
            auto view_model = make(locale);
            view_model->beginVisit();
            return view_model;
        }

        // What Save would hand to the application boundary right now.
        GraphConfiguration saved(SettingsViewModel &view_model) {
            configuration->saves.clear();
            view_model.save();
            return configuration->saves.empty() ? GraphConfiguration{} : configuration->saves.back();
        }

        static bool hasIssue(const QVariantList &issues, const QString &graph, const QString &group,
                             const QString &target, const QString &field, const QString &code) {
            for (const auto &value: issues) {
                const auto issue = value.toMap();
                if (issue["graph"] == graph && issue["group"] == group && issue["target"] == target &&
                    issue["field"] == field && issue["code"] == code)
                    return true;
            }
            return false;
        }

        static QString qstring(const std::string_view text) {
            return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
        }

        static QString nameOf(const GraphKey key) { return qstring(graphKeyName(key)); }

        static QString modeOf(const AxisPolicy &policy) {
            if (std::holds_alternative<StartAtZero>(policy)) return "zero";
            return std::holds_alternative<FixedBounds>(policy) ? "fixed" : "fit";
        }

        static QVariantMap seriesEntry(const QVariantMap &draft, const QString &key) {
            for (const auto &value: draft["series"].toList())
                if (value.toMap()["key"] == key) return value.toMap();
            return {};
        }

        static QVariantMap axisEntry(const QVariantMap &draft, const QString &key) {
            for (const auto &value: draft["axes"].toList())
                if (value.toMap()["key"] == key) return value.toMap();
            return {};
        }
    };

    TEST_F(GraphDraftTest, EachGraphDraftDescribesOnlyTheControlsItOffers) {
        const auto view_model = visit();

        for (const auto key: kGraphKeys) {
            SCOPED_TRACE(nameOf(key).toStdString());
            const auto &descriptor = graphDescriptor(key);
            const auto draft = view_model->graphDraft(nameOf(key));

            EXPECT_EQ(draft["hasRange"].toBool(), descriptor.dateRange);
            EXPECT_EQ(draft["hasLatestRuns"].toBool(), descriptor.latestRuns);
            EXPECT_EQ(draft["hasWindow"].toBool(), descriptor.averagingWindow);
            const auto series = draft["series"].toList();
            ASSERT_EQ(series.size(), static_cast<qsizetype>(descriptor.series.size()));
            for (std::size_t i = 0; i < descriptor.series.size(); ++i)
                EXPECT_EQ(series[static_cast<qsizetype>(i)].toMap()["key"].toString(), qstring(descriptor.series[i].key));
            const auto axes = draft["axes"].toList();
            ASSERT_EQ(axes.size(), static_cast<qsizetype>(descriptor.axes.size()));
            for (std::size_t i = 0; i < descriptor.axes.size(); ++i)
                EXPECT_EQ(axes[static_cast<qsizetype>(i)].toMap()["key"].toString(), qstring(descriptor.axes[i].key));
        }
    }

    TEST_F(GraphDraftTest, AnAxisNamesTheSeriesThatShareIt) {
        const auto view_model = visit();
        const auto &descriptor = graphDescriptor(GraphKey::ScenarioHistory);
        const auto draft = view_model->graphDraft("scenarioHistory");

        ASSERT_FALSE(descriptor.axes.empty());
        for (const auto &axis: descriptor.axes) {
            QStringList sharing;
            for (const auto &series: descriptor.series)
                if (series.axisKey == axis.key) sharing.append(qstring(series.key));
            const auto entry = axisEntry(draft, qstring(axis.key));

            EXPECT_EQ(entry["seriesKeys"].toStringList(), sharing) << axis.key;
            EXPECT_EQ(entry["mode"].toString(), modeOf(axis.defaultPolicy)) << axis.key;
        }
    }

    TEST_F(GraphDraftTest, TheDraftOfAFreshVisitShowsTheDefaults) {
        const auto view_model = visit();
        const auto dailyDefaults = defaultGraphPreferences(GraphKey::DailyPlaytime);
        const auto historyDefaults = defaultGraphPreferences(GraphKey::ScenarioHistory);
        ASSERT_TRUE(dailyDefaults.range && std::holds_alternative<RecentDays>(dailyDefaults.range->range));
        ASSERT_TRUE(dailyDefaults.averagingWindowDays);
        ASSERT_TRUE(historyDefaults.range && std::holds_alternative<LatestRuns>(historyDefaults.range->range));

        const auto daily = view_model->graphDraft("dailyPlaytime");
        const auto history = view_model->graphDraft("scenarioHistory");

        EXPECT_EQ(daily["rangeKind"].toString(), "recentDays");
        EXPECT_EQ(daily["anchor"].toBool(), dailyDefaults.range->anchorToLatestRun);
        EXPECT_EQ(daily["windowText"].toString(), english().toString(*dailyDefaults.averagingWindowDays));
        EXPECT_EQ(history["rangeKind"].toString(), "latestRuns");
        EXPECT_EQ(daily["connection"].toString(), qstring(connectionModeName(dailyDefaults.appearance.connection)));
        EXPECT_EQ(daily["markers"].toBool(), dailyDefaults.appearance.lineModeMarkers);
        EXPECT_EQ(history["markers"].toBool(), historyDefaults.appearance.lineModeMarkers);
        const auto score = seriesEntry(history, "score");
        const auto scoreStyle = defaultSeriesStyle(GraphKey::ScenarioHistory, "score");
        ASSERT_TRUE(scoreStyle.color);
        EXPECT_FALSE(score["colorOverridden"].toBool());
        EXPECT_FALSE(score["widthOverridden"].toBool());
        EXPECT_EQ(score["widthText"].toString(), english().toString(scoreStyle.width));
        EXPECT_EQ(score["color"].value<QColor>(),
                  QColor(scoreStyle.color->red, scoreStyle.color->green, scoreStyle.color->blue,
                         scoreStyle.color->alpha));
        EXPECT_EQ(seriesEntry(view_model->graphDraft("benchmarkRank"), "rank")["color"].value<QColor>().isValid(),
                  defaultSeriesStyle(GraphKey::BenchmarkRank, "rank").color.has_value());
        EXPECT_TRUE(view_model->draftIssues().isEmpty());
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(GraphDraftTest, OutsideAVisitTheDraftsAreTheCommittedValuesAndEditsAreRefused) {
        configuration->configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays = 9;
        const auto view_model = make();

        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["windowText"].toString(), "9");
        EXPECT_FALSE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "5"));
        EXPECT_FALSE(view_model->setGraphMarkers("dailyPlaytime", true));
        EXPECT_FALSE(view_model->resetGraph("dailyPlaytime"));
        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["windowText"].toString(), "9");
    }

    TEST_F(GraphDraftTest, DraftEditsNotifyAndStayOutOfTheApplicationUntilSave) {
        const auto view_model = visit();
        const auto committed = configuration->configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays;
        ASSERT_TRUE(committed);
        const int edited = *committed + 1;
        const QSignalSpy draftSpy(view_model.get(), &SettingsViewModel::graphDraftsChanged);
        const QSignalSpy pendingSpy(view_model.get(), &SettingsViewModel::pendingChangesChanged);

        EXPECT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow",
                                              QString::number(edited)));

        EXPECT_EQ(draftSpy.count(), 1);
        EXPECT_EQ(pendingSpy.count(), 1);
        EXPECT_TRUE(view_model->hasPendingChanges());
        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["windowText"].toString(), QString::number(edited));
        EXPECT_TRUE(configuration->saves.empty());
        EXPECT_TRUE(configuration->patches.empty());
        EXPECT_EQ(configuration->configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays, committed);
        EXPECT_EQ(view_model->quickSettings()["dailyPlaytime"].toMap()["window"].toInt(), *committed);
    }

    TEST_F(GraphDraftTest, AnEditThatChangesNothingNotifiesNothing) {
        const auto view_model = visit();
        const auto before = view_model->graphDraft("dailyPlaytime");
        const QSignalSpy draftSpy(view_model.get(), &SettingsViewModel::graphDraftsChanged);

        EXPECT_TRUE(view_model->setGraphMarkers("dailyPlaytime", before["markers"].toBool()));
        EXPECT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow",
                                              before["windowText"].toString()));

        EXPECT_EQ(draftSpy.count(), 0);
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(GraphDraftTest, RangeKindsStoreTheirCountsAndKeepTheAnchorFlag) {
        const auto view_model = visit();
        ASSERT_TRUE(view_model->setGraphAnchor("scenarioHistory", true));

        ASSERT_TRUE(view_model->setGraphRangeKind("scenarioHistory", "recentDays"));
        ASSERT_TRUE(view_model->setGraphField("scenarioHistory", "dataRange", "", "recentDays", "45"));
        auto range = saved(*view_model).preferences(GraphKey::ScenarioHistory).range;
        ASSERT_TRUE(range);
        EXPECT_EQ(std::get<RecentDays>(range->range).days, 45);
        EXPECT_TRUE(range->anchorToLatestRun);

        ASSERT_TRUE(view_model->setGraphRangeKind("scenarioHistory", "latestRuns"));
        ASSERT_TRUE(view_model->setGraphField("scenarioHistory", "dataRange", "", "latestRuns", "60"));
        range = saved(*view_model).preferences(GraphKey::ScenarioHistory).range;
        EXPECT_EQ(std::get<LatestRuns>(range->range).count, 60);
        EXPECT_TRUE(range->anchorToLatestRun);

        ASSERT_TRUE(view_model->setGraphRangeKind("scenarioHistory", "allHistory"));
        range = saved(*view_model).preferences(GraphKey::ScenarioHistory).range;
        EXPECT_TRUE(std::holds_alternative<AllHistory>(range->range));
        EXPECT_TRUE(range->anchorToLatestRun);
        EXPECT_EQ(view_model->graphDraft("scenarioHistory")["rangeKind"].toString(), "allHistory");
    }

    TEST_F(GraphDraftTest, ARangeKindTheGraphDoesNotOfferIsRefused) {
        const auto view_model = visit();

        EXPECT_FALSE(view_model->setGraphRangeKind("dailyPlaytime", "latestRuns"));
        EXPECT_FALSE(view_model->setGraphRangeKind("run", "recentDays"));
        EXPECT_FALSE(view_model->setGraphRangeKind("dailyPlaytime", "fortnight"));
        EXPECT_FALSE(view_model->setGraphAnchor("run", true));
        EXPECT_FALSE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "latestRuns", "5"));
        EXPECT_FALSE(view_model->setGraphField("run", "dataRange", "", "recentDays", "5"));
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(GraphDraftTest, CustomDatesAreCalendarDatesReadInTheAppLocale) {
        const auto view_model = visit(german());
        ASSERT_TRUE(view_model->setGraphRangeKind("dailyPlaytime", "customDates"));

        EXPECT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "startDate", "05.01.2026"));
        EXPECT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "endDate", "28.02.2026"));

        const auto range = saved(*view_model).preferences(GraphKey::DailyPlaytime).range;
        const auto &custom = std::get<CustomDates>(range->range);
        EXPECT_EQ(custom.start, std::chrono::year{2026} / std::chrono::January / 5);
        EXPECT_EQ(custom.end, std::chrono::year{2026} / std::chrono::February / 28);
        const auto draft = view_model->graphDraft("dailyPlaytime");
        EXPECT_EQ(draft["rangeStart"].toString(), "2026-01-05");
        EXPECT_EQ(draft["rangeEndText"].toString(), "28.02.2026");
        EXPECT_EQ(draft["rangeStartFormatted"].toString(), "05.01.2026");
    }

    TEST_F(GraphDraftTest, AnIsoDateIsAcceptedInAnyLocale) {
        const auto view_model = visit(german());
        ASSERT_TRUE(view_model->setGraphRangeKind("dailyPlaytime", "customDates"));

        EXPECT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "startDate", "2026-03-09"));

        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["rangeStart"].toString(), "2026-03-09");
    }

    TEST_F(GraphDraftTest, ADateIsReadInTheLocalesOrderNotAnother) {
        const auto view_model = visit(english());
        ASSERT_TRUE(view_model->setGraphRangeKind("dailyPlaytime", "customDates"));

        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "startDate", "3/9/2026"));

        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["rangeStart"].toString(), "2026-03-09");
        EXPECT_FALSE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "endDate", "28.02.2026"));
    }

    TEST_F(GraphDraftTest, DateTextThatDoesNotParseWholeIsKeptAsUnparsedInput) {
        const auto view_model = visit(german());
        ASSERT_TRUE(view_model->setGraphRangeKind("dailyPlaytime", "customDates"));
        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "endDate", "28.02.2026"));
        const QSignalSpy rawSpy(view_model.get(), &SettingsViewModel::rawInputsChanged);

        for (const QString &text: {"31.02.2026", "05.01.2026 morgen", "5.1.", "", "heute", "2026-13-01"}) {
            EXPECT_FALSE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "startDate", text))
                << text.toStdString();
            EXPECT_EQ(view_model->rawInput("dailyPlaytime", "dataRange", "", "startDate").toString(), text);
            EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["rangeStartText"].toString(), text);
            EXPECT_TRUE(hasIssue(view_model->draftIssues(), "dailyPlaytime", "dataRange", "", "startDate",
                                 "unparsedInput"));
        }

        EXPECT_GE(rawSpy.count(), 1);
        EXPECT_TRUE(configuration->saves.empty());
        EXPECT_FALSE(view_model->save()["accepted"].toBool());
        EXPECT_TRUE(configuration->saves.empty());
    }

    TEST_F(GraphDraftTest, ACompleteParseClearsTheUnparsedInputAndItsIssue) {
        const auto view_model = visit(german());
        ASSERT_TRUE(view_model->setGraphRangeKind("dailyPlaytime", "customDates"));
        ASSERT_FALSE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "startDate", "5.1."));

        EXPECT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "startDate", "05.01.2026"));

        EXPECT_FALSE(view_model->rawInput("dailyPlaytime", "dataRange", "", "startDate").isValid());
        EXPECT_FALSE(hasIssue(view_model->draftIssues(), "dailyPlaytime", "dataRange", "", "startDate",
                              "unparsedInput"));
    }

    TEST_F(GraphDraftTest, AReversedOrUnsupportedDateRangeIsReportedNextToTheDateAtFault) {
        const auto view_model = visit(english());
        ASSERT_TRUE(view_model->setGraphRangeKind("dailyPlaytime", "customDates"));
        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "startDate", "3/9/2026"));

        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "endDate", "3/8/2026"));
        EXPECT_TRUE(hasIssue(view_model->draftIssues(), "dailyPlaytime", "dataRange", "", "endDate",
                             "reversedDateRange"));

        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "endDate", "3/9/2026"));
        EXPECT_TRUE(view_model->draftIssues().isEmpty());

        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "startDate", "12/31/1969"));
        EXPECT_TRUE(hasIssue(view_model->draftIssues(), "dailyPlaytime", "dataRange", "", "startDate",
                             "invalidDate"));
    }

    TEST_F(GraphDraftTest, CountsMustParseAsWholeNumbersInTheAppLocale) {
        const auto view_model = visit(english());
        ASSERT_TRUE(view_model->setGraphRangeKind("scenarioHistory", "latestRuns"));

        for (const QString &text: {"3x", "3.5", "", "ten", "1e2", "99999999999999999999"}) {
            EXPECT_FALSE(view_model->setGraphField("scenarioHistory", "dataRange", "", "latestRuns", text))
                << text.toStdString();
            EXPECT_EQ(view_model->rawInput("scenarioHistory", "dataRange", "", "latestRuns").toString(), text);
        }
        EXPECT_TRUE(view_model->setGraphField("scenarioHistory", "dataRange", "", "latestRuns", " 42 "));

        EXPECT_FALSE(view_model->rawInput("scenarioHistory", "dataRange", "", "latestRuns").isValid());
        EXPECT_EQ(std::get<LatestRuns>(saved(*view_model).preferences(GraphKey::ScenarioHistory).range->range).count,
                  42);
    }

    TEST_F(GraphDraftTest, ACountOutsideTheLimitsParsesAndIsReportedBySemanticValidation) {
        const auto view_model = visit(english());
        ASSERT_TRUE(view_model->setGraphRangeKind("dailyPlaytime", "recentDays"));

        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "recentDays", "0"));
        EXPECT_TRUE(hasIssue(view_model->draftIssues(), "dailyPlaytime", "dataRange", "", "recentDays",
                             "nonPositiveCount"));
        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "recentDays", "36501"));
        EXPECT_TRUE(hasIssue(view_model->draftIssues(), "dailyPlaytime", "dataRange", "", "recentDays",
                             "countTooLarge"));
        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "recentDays", "36500"));
        EXPECT_TRUE(view_model->draftIssues().isEmpty());

        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "-2"));
        EXPECT_TRUE(hasIssue(view_model->draftIssues(), "dailyPlaytime", "dataRange", "", "averagingWindow",
                             "nonPositiveCount"));
    }

    TEST_F(GraphDraftTest, DecimalNumbersFollowTheAppLocale) {
        const auto de = visit(german());
        EXPECT_TRUE(de->setGraphField("scenarioHistory", "seriesStyle", "score", "width", "2,5"));
        EXPECT_EQ(saved(*de).preferences(GraphKey::ScenarioHistory).seriesStyles.at("score").width, 2.5);
        EXPECT_EQ(seriesEntry(de->graphDraft("scenarioHistory"), "score")["widthText"].toString(), "2,5");
        EXPECT_FALSE(de->setGraphField("scenarioHistory", "seriesStyle", "hits", "width", "2.5"));
        EXPECT_EQ(de->rawInput("scenarioHistory", "seriesStyle", "hits", "width").toString(), "2.5");

        const auto us = visit(english());
        EXPECT_TRUE(us->setGraphField("scenarioHistory", "seriesStyle", "score", "width", "2.5"));
        EXPECT_FALSE(us->setGraphField("scenarioHistory", "seriesStyle", "hits", "width", "2,5"));
    }

    TEST_F(GraphDraftTest, NonFiniteNumbersAreNeverStored) {
        const auto view_model = visit(english());

        for (const QString &text: {"inf", "nan", "-inf", "1e999"}) {
            EXPECT_FALSE(view_model->setGraphField("scenarioHistory", "seriesStyle", "score", "width", text))
                << text.toStdString();
            EXPECT_TRUE(view_model->rawInput("scenarioHistory", "seriesStyle", "score", "width").isValid());
        }
        EXPECT_EQ(view_model->draftIssues().size(), 1);
    }

    TEST_F(GraphDraftTest, ALineWidthOutsideTheLimitsIsReportedAtItsSeries) {
        const auto view_model = visit(english());

        ASSERT_TRUE(view_model->setGraphField("scenarioHistory", "seriesStyle", "accuracy", "width", "0.4"));
        EXPECT_TRUE(hasIssue(view_model->draftIssues(), "scenarioHistory", "seriesStyle", "accuracy", "width",
                             "lineWidthOutOfRange"));
        ASSERT_TRUE(view_model->setGraphField("scenarioHistory", "seriesStyle", "accuracy", "width", "12.5"));
        EXPECT_TRUE(hasIssue(view_model->draftIssues(), "scenarioHistory", "seriesStyle", "accuracy", "width",
                             "lineWidthOutOfRange"));
        ASSERT_TRUE(view_model->setGraphField("scenarioHistory", "seriesStyle", "accuracy", "width", "12"));
        EXPECT_TRUE(view_model->draftIssues().isEmpty());
    }

    TEST_F(GraphDraftTest, SeriesColourAndWidthOverridesSetAndClearAndAnEmptyOverrideLeavesNoTrace) {
        const auto view_model = visit(english());
        const auto defaults = defaultSeriesStyle(GraphKey::ScenarioHistory, "hits");
        ASSERT_TRUE(defaults.color);
        const QColor defaultColor(defaults.color->red, defaults.color->green, defaults.color->blue,
                                  defaults.color->alpha);
        const double overrideWidth = defaults.width + 2.0;

        ASSERT_TRUE(view_model->setSeriesColor("scenarioHistory", "hits", QColor(10, 20, 30, 255)));
        auto hits = seriesEntry(view_model->graphDraft("scenarioHistory"), "hits");
        EXPECT_TRUE(hits["colorOverridden"].toBool());
        EXPECT_EQ(hits["color"].value<QColor>(), QColor(10, 20, 30, 255));
        ASSERT_TRUE(view_model->setGraphField("scenarioHistory", "seriesStyle", "hits", "width",
                                              english().toString(overrideWidth)));
        EXPECT_TRUE(seriesEntry(view_model->graphDraft("scenarioHistory"), "hits")["widthOverridden"].toBool());
        const auto stored = saved(*view_model).preferences(GraphKey::ScenarioHistory).seriesStyles.at("hits");
        EXPECT_EQ(stored.color, (RgbaColor{10, 20, 30, 255}));
        EXPECT_EQ(stored.width, overrideWidth);

        ASSERT_TRUE(view_model->clearSeriesOverride("scenarioHistory", "hits", "color"));
        ASSERT_TRUE(view_model->clearSeriesOverride("scenarioHistory", "hits", "width"));

        hits = seriesEntry(view_model->graphDraft("scenarioHistory"), "hits");
        EXPECT_FALSE(hits["colorOverridden"].toBool());
        EXPECT_EQ(hits["color"].value<QColor>(), defaultColor);
        EXPECT_EQ(hits["widthText"].toString(), english().toString(defaults.width));
        EXPECT_TRUE(saved(*view_model).preferences(GraphKey::ScenarioHistory).seriesStyles.empty());
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(GraphDraftTest, ClearingAWidthAlsoDropsItsUnparsedText) {
        const auto view_model = visit(english());
        ASSERT_FALSE(view_model->setGraphField("scenarioHistory", "seriesStyle", "hits", "width", "wide"));

        EXPECT_TRUE(view_model->clearSeriesOverride("scenarioHistory", "hits", "width"));

        EXPECT_FALSE(view_model->rawInput("scenarioHistory", "seriesStyle", "hits", "width").isValid());
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(GraphDraftTest, SeriesStylesOfAnUnknownSeriesOrAGraphWithoutOnesAreRefused) {
        const auto view_model = visit();

        EXPECT_FALSE(view_model->setSeriesColor("scenarioHistory", "nothing", QColor("red")));
        EXPECT_FALSE(view_model->setSeriesColor("run", "score", QColor("red")));
        EXPECT_FALSE(view_model->setGraphField("scenarioHistory", "seriesStyle", "nothing", "width", "4"));
        EXPECT_FALSE(view_model->clearSeriesOverride("scenarioHistory", "score", "dash"));
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(GraphDraftTest, AxisModesAndFixedBoundsAreEditedInDisplayedUnits) {
        const auto view_model = visit(english());

        ASSERT_TRUE(view_model->setAxisMode("scenarioHistory", "accuracy", "fixed"));
        ASSERT_TRUE(view_model->setGraphField("scenarioHistory", "axes", "accuracy", "fixedMin", "80"));
        ASSERT_TRUE(view_model->setGraphField("scenarioHistory", "axes", "accuracy", "fixedMax", "99.5"));

        const auto &policy = saved(*view_model).preferences(GraphKey::ScenarioHistory).axisPolicies.at("accuracy");
        const auto &bounds = std::get<FixedBounds>(policy);
        EXPECT_EQ(bounds.minimum, 80.0);
        EXPECT_EQ(bounds.maximum, 99.5);
        const auto accuracy = axisEntry(view_model->graphDraft("scenarioHistory"), "accuracy");
        EXPECT_EQ(accuracy["mode"].toString(), "fixed");
        EXPECT_EQ(accuracy["minText"].toString(), "80");
        EXPECT_EQ(accuracy["maxText"].toString(), "99.5");

        ASSERT_TRUE(view_model->setAxisMode("scenarioHistory", "accuracy", "zero"));
        EXPECT_TRUE(std::holds_alternative<StartAtZero>(
            saved(*view_model).preferences(GraphKey::ScenarioHistory).axisPolicies.at("accuracy")));
    }

    TEST_F(GraphDraftTest, ReturningAnAxisToItsSavedModeLeavesTheVisitClean) {
        const auto view_model = visit(english());

        ASSERT_TRUE(view_model->setAxisMode("scenarioHistory", "accuracy", "fixed"));
        ASSERT_TRUE(view_model->hasPendingChanges());
        ASSERT_TRUE(view_model->setAxisMode("scenarioHistory", "accuracy", "fit"));

        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(GraphDraftTest, FixedBoundsThatDoNotIncreaseAreReportedAtTheMaximum) {
        const auto view_model = visit(english());
        ASSERT_TRUE(view_model->setAxisMode("dailyPlaytime", "playtime", "fixed"));
        EXPECT_TRUE(hasIssue(view_model->draftIssues(), "dailyPlaytime", "axes", "playtime", "fixedMax",
                             "fixedBoundsNotIncreasing"));

        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "axes", "playtime", "fixedMin", "10"));
        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "axes", "playtime", "fixedMax", "10"));
        EXPECT_TRUE(hasIssue(view_model->draftIssues(), "dailyPlaytime", "axes", "playtime", "fixedMax",
                             "fixedBoundsNotIncreasing"));

        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "axes", "playtime", "fixedMax", "90"));
        EXPECT_TRUE(view_model->draftIssues().isEmpty());
        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "axes", "playtime", "fixedMax", "2e12"));
        EXPECT_TRUE(hasIssue(view_model->draftIssues(), "dailyPlaytime", "axes", "playtime", "fixedMax",
                             "axisBoundOutOfRange"));
    }

    TEST_F(GraphDraftTest, BoundTextIsOnlyAcceptedWhileTheAxisIsFixed) {
        const auto view_model = visit(english());

        EXPECT_FALSE(view_model->setGraphField("scenarioHistory", "axes", "score", "fixedMin", "5"));
        EXPECT_FALSE(view_model->setGraphField("benchmarkRank", "axes", "rank", "fixedMin", "5"));
        EXPECT_FALSE(view_model->setAxisMode("benchmarkRank", "rank", "fixed"));
        EXPECT_FALSE(view_model->setAxisMode("run", "score", "fit"));
        EXPECT_FALSE(view_model->setAxisMode("scenarioHistory", "score", "logarithmic"));
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(GraphDraftTest, LeavingAKindOrModeDropsTheUnparsedTextOfTheFieldsItHides) {
        const auto view_model = visit(english());
        ASSERT_TRUE(view_model->setGraphRangeKind("scenarioHistory", "latestRuns"));
        ASSERT_FALSE(view_model->setGraphField("scenarioHistory", "dataRange", "", "latestRuns", "ten"));
        ASSERT_TRUE(view_model->setAxisMode("scenarioHistory", "score", "fixed"));
        ASSERT_FALSE(view_model->setGraphField("scenarioHistory", "axes", "score", "fixedMin", "low"));
        ASSERT_EQ(view_model->rawInputs().size(), 2);

        ASSERT_TRUE(view_model->setGraphRangeKind("scenarioHistory", "allHistory"));
        ASSERT_TRUE(view_model->setAxisMode("scenarioHistory", "score", "fit"));

        EXPECT_TRUE(view_model->rawInputs().isEmpty());
        EXPECT_TRUE(view_model->draftIssues().isEmpty());
        ASSERT_TRUE(view_model->setGraphRangeKind("scenarioHistory", "latestRuns"));
        ASSERT_TRUE(view_model->setGraphField("scenarioHistory", "dataRange", "", "latestRuns", "100"));
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(GraphDraftTest, ConnectionAndMarkersAreEditedPerGraphAndKeepTheMarkerPreference) {
        const auto view_model = visit();

        ASSERT_TRUE(view_model->setGraphMarkers("dailyPlaytime", true));
        ASSERT_TRUE(view_model->setGraphConnection("dailyPlaytime", "pointsOnly"));
        auto stored = saved(*view_model).preferences(GraphKey::DailyPlaytime).appearance;
        EXPECT_EQ(stored.connection, ConnectionMode::PointsOnly);
        EXPECT_TRUE(stored.lineModeMarkers);
        ASSERT_TRUE(view_model->setGraphMarkers("dailyPlaytime", false));
        ASSERT_TRUE(view_model->setGraphConnection("dailyPlaytime", "straight"));

        stored = saved(*view_model).preferences(GraphKey::DailyPlaytime).appearance;
        EXPECT_EQ(stored.connection, ConnectionMode::Straight);
        EXPECT_FALSE(stored.lineModeMarkers);
        EXPECT_EQ(saved(*view_model).preferences(GraphKey::Run).appearance, LineAppearance{});
        EXPECT_FALSE(view_model->setGraphConnection("dailyPlaytime", "wavy"));
        EXPECT_TRUE(view_model->setGraphConnection("run", "straight"));
    }

    TEST_F(GraphDraftTest, EditsToSeveralGraphsAreHeldTogetherAndSurviveNavigation) {
        const auto view_model = visit(english());
        const QString editedWindow = QString::number(kDefaultAveragingWindowDays + 1);
        view_model->setSelectedGraph("dailyPlaytime");
        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", editedWindow));
        view_model->setSelectedGraph("scenarioHistory");
        ASSERT_TRUE(view_model->setGraphRangeKind("scenarioHistory", "latestRuns"));
        ASSERT_FALSE(view_model->setGraphField("scenarioHistory", "dataRange", "", "latestRuns", "5o"));
        view_model->setSelectedGraph("benchmarkPlaytime");
        ASSERT_TRUE(view_model->setSeriesColor("benchmarkPlaytime", "playtime", QColor("#112233")));
        view_model->setSelectedGraph("dailyPlaytime");

        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["windowText"].toString(), editedWindow);
        EXPECT_EQ(view_model->graphDraft("scenarioHistory")["rangeCountText"].toString(), "5o");
        EXPECT_TRUE(seriesEntry(view_model->graphDraft("benchmarkPlaytime"), "playtime")["colorOverridden"].toBool());
        EXPECT_FALSE(view_model->graphDraft("benchmarkRank")["markers"].toBool());
        EXPECT_TRUE(view_model->hasPendingChanges());
    }

    TEST_F(GraphDraftTest, AnInvalidFieldOnAnotherGraphBlocksSaveButKeepsEveryEdit) {
        const auto view_model = visit(english());
        const int editedWindow = kDefaultAveragingWindowDays + 1;
        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow",
                                              QString::number(editedWindow)));
        ASSERT_FALSE(view_model->setGraphField("scenarioHistory", "seriesStyle", "hits", "width", "wide"));
        view_model->setSelectedGraph("benchmarkRank");

        const auto result = view_model->save();

        EXPECT_FALSE(result["accepted"].toBool());
        EXPECT_TRUE(configuration->saves.empty());
        ASSERT_EQ(result["issues"].toList().size(), 1);
        EXPECT_TRUE(hasIssue(result["issues"].toList(), "scenarioHistory", "seriesStyle", "hits", "width",
                             "unparsedInput"));
        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["windowText"].toString(), QString::number(editedWindow));
        EXPECT_EQ(seriesEntry(view_model->graphDraft("scenarioHistory"), "hits")["widthText"].toString(), "wide");
        EXPECT_TRUE(view_model->hasPendingChanges());

        ASSERT_TRUE(view_model->setGraphField("scenarioHistory", "seriesStyle", "hits", "width", "4"));
        EXPECT_TRUE(view_model->save()["accepted"].toBool());
        EXPECT_EQ(configuration->saves.back().preferences(GraphKey::DailyPlaytime).averagingWindowDays, editedWindow);
        EXPECT_EQ(configuration->saves.back().preferences(GraphKey::ScenarioHistory).seriesStyles.at("hits").width,
                  4.0);
    }

    TEST_F(GraphDraftTest, ResetStagesOnlyTheSelectedGraphsPresentationDefaults) {
        auto &history = configuration->configuration.preferences(GraphKey::ScenarioHistory);
        history.range = RangePreference{LatestRuns{20}, true};
        history.appearance = {ConnectionMode::PointsOnly, false};
        history.seriesStyles["score"].width = 8.0;
        history.axisPolicies["score"] = FixedBounds{1.0, 2.0};
        configuration->configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays = 9;
        const auto &lineup = configuration->configuration.runSeries.series;
        ASSERT_GE(lineup.size(), 2U);
        const auto lineCount = lineup.size();
        const auto toggledLine = lineup.front().id;
        const bool toggledEnabled = !lineup.front().presentation.enabled;
        const auto label = runAxisLabelFor(configuration->configuration.runSeries, lineup.back().id);
        configuration->configuration.runAxisLabel = label;
        const auto view_model = visit(english());
        ASSERT_TRUE(view_model->setSeriesEnabled(QString::number(toggledLine.value), toggledEnabled)["succeeded"].toBool());
        ASSERT_FALSE(view_model->setGraphField("scenarioHistory", "seriesStyle", "hits", "width", "wide"));
        ASSERT_FALSE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "nine"));

        ASSERT_TRUE(view_model->resetGraph("scenarioHistory"));

        EXPECT_FALSE(view_model->rawInput("scenarioHistory", "seriesStyle", "hits", "width").isValid());
        EXPECT_EQ(view_model->rawInput("dailyPlaytime", "dataRange", "", "averagingWindow").toString(), "nine");
        view_model->clearRawInput("dailyPlaytime", "dataRange", "", "averagingWindow");
        const auto candidate = saved(*view_model);
        EXPECT_EQ(candidate.preferences(GraphKey::ScenarioHistory), defaultGraphPreferences(GraphKey::ScenarioHistory));
        EXPECT_EQ(candidate.preferences(GraphKey::DailyPlaytime).averagingWindowDays, 9);
        EXPECT_EQ(candidate.runAxisLabel, label);
        ASSERT_EQ(candidate.runSeries.series.size(), lineCount);
        EXPECT_EQ(candidate.runSeries.series.front().presentation.enabled, toggledEnabled)
            << "a Graph Lines edit survives the reset";
    }

    TEST_F(GraphDraftTest, ResettingTheRunGraphNeverTouchesItsGraphLines) {
        const auto &lineup = configuration->configuration.runSeries;
        ASSERT_GE(lineup.series.size(), 2U);
        const auto lineCount = static_cast<qsizetype>(lineup.series.size());
        const auto axisCount = static_cast<qsizetype>(lineup.axes.size());
        const auto editedLine = QString::number(lineup.series.front().id.value);
        const auto view_model = visit();
        const auto axis = view_model->createAxis("Mine");
        ASSERT_TRUE(axis["succeeded"].toBool());
        ASSERT_TRUE(view_model->updateSeriesAxis(editedLine, axis["createdAxisId"].toString())["succeeded"].toBool());
        ASSERT_TRUE(view_model->createComputedSeries("Extra", "#123456", 2.0, true, "")["succeeded"].toBool());
        const auto label = "axis:" + axis["createdAxisId"].toString();
        view_model->setRunAxisLabel(label);
        ASSERT_TRUE(view_model->setGraphConnection("run", "straight"));
        ASSERT_TRUE(view_model->setGraphMarkers("run", true));

        ASSERT_TRUE(view_model->resetGraph("run"));

        EXPECT_EQ(view_model->graphDraft("run")["connection"].toString(), "smooth");
        EXPECT_FALSE(view_model->graphDraft("run")["markers"].toBool());
        EXPECT_EQ(view_model->getAllAxes().size(), axisCount + 1);
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), lineCount + 1);
        EXPECT_EQ(view_model->runAxisLabelKey(), label);
        EXPECT_TRUE(view_model->hasPendingChanges());
    }

    TEST_F(GraphDraftTest, AResetIsCancellableAndNothingReachesTheApplicationBeforeSave) {
        const auto defaultWindow = defaultGraphPreferences(GraphKey::DailyPlaytime).averagingWindowDays;
        ASSERT_TRUE(defaultWindow);
        const int committedWindow = *defaultWindow + 2;
        configuration->configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays = committedWindow;
        const auto view_model = visit();

        ASSERT_TRUE(view_model->resetGraph("dailyPlaytime"));
        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["windowText"].toString(), english().toString(*defaultWindow));
        EXPECT_TRUE(view_model->hasPendingChanges());
        EXPECT_TRUE(configuration->saves.empty());

        view_model->revertChanges();

        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["windowText"].toString(), english().toString(committedWindow));
        EXPECT_FALSE(view_model->hasPendingChanges());
        EXPECT_FALSE(view_model->resetGraph("nonsense"));
    }

    TEST_F(GraphDraftTest, ResettingADefaultGraphStagesNothing) {
        const auto view_model = visit();

        ASSERT_TRUE(view_model->resetGraph("scenarioHistory"));

        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(GraphDraftTest, RevertAndEndVisitDropTheDraftTextAndItsIssues) {
        const auto view_model = visit(english());
        const auto before = view_model->graphDraft("dailyPlaytime");
        const bool editedMarkers = !before["markers"].toBool();
        ASSERT_FALSE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "x"));
        ASSERT_TRUE(view_model->setGraphMarkers("dailyPlaytime", editedMarkers));
        ASSERT_FALSE(view_model->draftIssues().isEmpty());

        view_model->revertChanges();

        EXPECT_TRUE(view_model->draftIssues().isEmpty());
        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["windowText"].toString(), before["windowText"].toString());
        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["markers"].toBool(), before["markers"].toBool());

        ASSERT_TRUE(view_model->setGraphMarkers("dailyPlaytime", editedMarkers));
        view_model->endVisit();
        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["markers"].toBool(), before["markers"].toBool());
    }

    TEST_F(GraphDraftTest, TheDraftsFollowCommittedChangesOutsideAVisitAndIgnoreThemDuringOne) {
        const auto view_model = make();
        const QSignalSpy spy(view_model.get(), &SettingsViewModel::graphDraftsChanged);

        configuration->configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays = 5;
        configuration->publish({GraphKey::DailyPlaytime}, false, false);
        EXPECT_GE(spy.count(), 1);
        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["windowText"].toString(), "5");

        view_model->beginVisit();
        configuration->configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays = 8;
        configuration->publish({GraphKey::DailyPlaytime}, false, false);
        EXPECT_EQ(view_model->graphDraft("dailyPlaytime")["windowText"].toString(), "5");
    }

    TEST_F(GraphDraftTest, TheLimitsAreStatedForTheControls) {
        const auto view_model = visit(german());

        const auto limits = view_model->graphLimits();

        EXPECT_EQ(limits["maxRecentDays"].toInt(), kMaximumRangeDays);
        EXPECT_EQ(limits["maxLatestRuns"].toInt(), kMaximumLatestRuns);
        EXPECT_EQ(limits["maxAveragingWindow"].toInt(), kMaximumAveragingWindowDays);
        EXPECT_EQ(limits["minWidth"].toDouble(), kMinimumLineWidth);
        EXPECT_EQ(limits["maxWidth"].toDouble(), kMaximumLineWidth);
        EXPECT_EQ(limits["maxBound"].toDouble(), kMaximumAxisBoundMagnitude);
        EXPECT_EQ(limits["earliestDate"].toString(), "1970-01-01");
        EXPECT_EQ(limits["latestDate"].toString(), "9999-12-31");
        EXPECT_EQ(limits["earliestDateText"].toString(), "01.01.1970");
        EXPECT_EQ(limits["dateFormat"].toString(), "dd.MM.yyyy");
    }

    TEST_F(GraphDraftTest, AnAnchorToggleAndRangeKindAreInTheDraftEntry) {
        const auto view_model = visit();
        const auto defaults = defaultGraphPreferences(GraphKey::BenchmarkRank);
        ASSERT_TRUE(defaults.range && std::holds_alternative<RecentDays>(defaults.range->range));
        const bool flippedAnchor = !defaults.range->anchorToLatestRun;
        ASSERT_TRUE(view_model->setGraphAnchor("benchmarkRank", flippedAnchor));
        ASSERT_TRUE(view_model->setGraphRangeKind("benchmarkRank", "allHistory"));

        auto draft = view_model->graphDraft("benchmarkRank");
        EXPECT_EQ(draft["anchor"].toBool(), flippedAnchor);
        EXPECT_EQ(draft["rangeKind"].toString(), "allHistory");

        ASSERT_TRUE(view_model->setGraphRangeKind("benchmarkRank", "recentDays"));

        draft = view_model->graphDraft("benchmarkRank");
        EXPECT_EQ(draft["anchor"].toBool(), flippedAnchor);
        EXPECT_EQ(draft["rangeKind"].toString(), "recentDays");
        EXPECT_GT(draft["rangeCount"].toInt(), 0);
        EXPECT_EQ(draft["rangeCountText"].toString().toInt(), draft["rangeCount"].toInt());
    }

    TEST_F(GraphDraftTest, SwitchingToCustomDatesSeedsAValidInclusiveRange) {
        const auto view_model = visit();

        ASSERT_TRUE(view_model->setGraphRangeKind("dailyPlaytime", "customDates"));

        const auto draft = view_model->graphDraft("dailyPlaytime");
        EXPECT_EQ(draft["rangeKind"].toString(), "customDates");
        EXPECT_FALSE(draft["rangeStart"].toString().isEmpty());
        EXPECT_LE(QDate::fromString(draft["rangeStart"].toString(), Qt::ISODate),
                  QDate::fromString(draft["rangeEnd"].toString(), Qt::ISODate));
        EXPECT_TRUE(view_model->draftIssues().isEmpty());
    }

    TEST_F(GraphDraftTest, TheDatesTheLimitsStateReadBackInEveryCommonLocale) {
        for (const auto &locale: {QLocale(QLocale::English, QLocale::UnitedStates), QLocale(QLocale::English, QLocale::UnitedKingdom),
                                  QLocale(QLocale::German, QLocale::Germany), QLocale(QLocale::French, QLocale::France),
                                  QLocale(QLocale::Japanese, QLocale::Japan), QLocale(QLocale::Swedish, QLocale::Sweden),
                                  QLocale(QLocale::Dutch, QLocale::Netherlands), QLocale(QLocale::Portuguese, QLocale::Brazil)}) {
            const auto view_model = visit(locale);
            ASSERT_TRUE(view_model->setGraphRangeKind("dailyPlaytime", "customDates"));
            const auto limits = view_model->graphLimits();

            EXPECT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "startDate",
                                                  limits["earliestDateText"].toString()))
                << locale.name().toStdString() << ": " << limits["earliestDateText"].toString().toStdString();
            EXPECT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "endDate",
                                                  limits["latestDateText"].toString()))
                << locale.name().toStdString() << ": " << limits["latestDateText"].toString().toStdString();
            const auto draft = view_model->graphDraft("dailyPlaytime");
            EXPECT_EQ(draft["rangeStart"].toString(), "1970-01-01") << locale.name().toStdString();
            EXPECT_EQ(draft["rangeEnd"].toString(), "9999-12-31") << locale.name().toStdString();
            EXPECT_EQ(draft["rangeStartText"].toString(), limits["earliestDateText"].toString());
        }
    }

    TEST_F(GraphDraftTest, FixingTheLastIssueOfARejectedSaveClearsItsList) {
        const auto view_model = visit(english());
        ASSERT_FALSE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "x"));
        ASSERT_FALSE(view_model->save()["accepted"].toBool());
        ASSERT_EQ(view_model->issues().size(), 1);

        ASSERT_TRUE(view_model->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "4"));

        EXPECT_TRUE(view_model->issues().isEmpty());
        EXPECT_TRUE(view_model->draftIssues().isEmpty());
    }

    TEST_F(GraphDraftTest, ASemanticIssueOfARejectedSaveStaysListedWhileTheDraftIsUntouched) {
        configuration->rejectSavesWith = {{GraphConfigurationIssueCode::NonPositiveCount, GraphKey::DailyPlaytime,
                                           "dataRange", "", "averagingWindow"}};
        const auto view_model = visit(english());
        ASSERT_TRUE(view_model->setGraphMarkers("scenarioHistory", false));

        EXPECT_FALSE(view_model->save()["accepted"].toBool());

        EXPECT_EQ(view_model->issues().size(), 1);
        EXPECT_TRUE(view_model->hasPendingChanges());
    }
}
