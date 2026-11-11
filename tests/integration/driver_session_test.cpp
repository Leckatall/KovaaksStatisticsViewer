//
// Session composition for the dev-tool UI driver (tools/driver): overlays onto
// a session's data directory and typed `--setting` assignments, checked against
// what the real SettingsService reads back. Built only with the dev tools.
//

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>

#include "benchmarks/benchmark_evaluator.h"
#include "driver_dataset.h"
#include "driver_ports.h"
#include "driver_spec.h"
#include "fake_settings_service.h"
#include "formats/csv/stats_csv_parser.h"
#include "formats/protobuf/profile_serializer.h"
#include "formats/protobuf/proto_decoder.h"
#include "qt_data/benchmark_store.h"
#include "qt_data/graph_configuration_persistence.h"
#include "session_compose.h"
#include "settings_service.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <variant>

using namespace ksv;

namespace {
    bool writeFile(const QString &path, const QByteArray &content) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
    }

    QByteArray readFile(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    QJsonObject json(const char *text) { return QJsonDocument::fromJson(text).object(); }

    QString specError(const char *text) {
        driver::ProfileSpec spec;
        return driver::parseProfileSpec(json(text), "spec.json", spec);
    }

    TEST(DriverSpecTest, BuildsTheSameProfileBytesEveryTime) {
        driver::ProfileSpec spec;
        ASSERT_TRUE(driver::parseProfileSpec(json(R"({"scenarios": [
            {"name": "A", "runs": 7, "runsPerDay": 3, "daySpacing": 2},
            {"name": "B", "hash": "b-hash", "runs": 2, "shape": "gallery-pasu", "durationSeconds": 30}]})"),
                                             "spec.json", spec).isEmpty());
        QTemporaryDir dir;
        const QString specPath = QDir(dir.path()).filePath("spec.json");
        ASSERT_TRUE(writeFile(specPath, R"({"scenarios": [{"name": "A", "runs": 7, "runsPerDay": 3, "daySpacing": 2},
            {"name": "B", "hash": "b-hash", "runs": 2, "shape": "gallery-pasu", "durationSeconds": 30}]})"));
        const QString first = QDir(dir.path()).filePath("first.pb");
        const QString second = QDir(dir.path()).filePath("second.pb");
        const QString benchmarks = QDir(dir.path()).filePath("benchmarks");
        ASSERT_TRUE(driver::writeProfileSpec(specPath, first, benchmarks).isEmpty());
        ASSERT_TRUE(driver::writeProfileSpec(specPath, second, benchmarks).isEmpty());

        // The store's created_at is pinned, so no clock reaches the bytes.
        EXPECT_EQ(readFile(first), readFile(second));
        const auto profile = driver::buildProfile(spec);
        ASSERT_EQ(profile.getAllRunRecords().size(), 9U);
        std::set<std::string> hashes;
        for (const auto &run: profile.getAllRunRecords()) hashes.insert(run.run_id.scenario_id.hash);
        EXPECT_EQ(hashes, (std::set<std::string>{"spec-a", "b-hash"}));
    }

    TEST(DriverSpecTest, PerformanceFreeRunsKeepOnlyTheirTotals) {
        driver::ProfileSpec spec;
        ASSERT_TRUE(driver::parseProfileSpec(json(R"({"scenarios": [{"name": "Totals", "runs": 2, "performance": false}]})"),
                                             "spec.json", spec).isEmpty());
        for (const auto &run: driver::buildProfile(spec).getAllRunRecords()) {
            EXPECT_FALSE(run.performance.has_value());
            EXPECT_GT(run.stored_totals.score, 0.0F);
        }
    }

    TEST(DriverSpecTest, ErrorsNameTheOffendingField) {
        EXPECT_TRUE(specError(R"({"scenarios": [{"name": "A", "runz": 3}]})").contains("scenarios[0].runz"));
        EXPECT_TRUE(specError(R"({"scenarios": [{"runs": 3}]})").contains("scenarios[0].name: is required"));
        EXPECT_TRUE(specError(R"({"scenarios": [{"name": "A", "runsPerDay": 0}]})").contains("scenarios[0].runsPerDay"));
        EXPECT_TRUE(specError(R"({"scenarios": [{"name": "A", "shape": "wobbly"}]})").contains("unknown shape"));
        EXPECT_TRUE(specError(R"({"scenarioz": []})").contains("scenarioz"));
        EXPECT_TRUE(specError(R"({"benchmark": {"categories": [{"name": "C", "scenarios": [{"hash": "x"}]}]}})")
                        .contains("benchmark.categories[0].scenarios[0].name"));
        EXPECT_TRUE(specError(R"({"benchmark": "missing.json"})").contains("no such file"));
    }

    TEST(DriverSpecTest, GeneratedBenchmarksResolveNamesAndLeaveUnknownsUnresolved) {
        driver::ProfileSpec spec;
        ASSERT_TRUE(driver::parseProfileSpec(json(R"({"scenarios": [{"name": "Known", "runs": 2},
            {"name": "Twin", "hash": "twin-a", "runs": 1}, {"name": "Twin", "hash": "twin-b", "runs": 1}],
            "benchmark": {"name": "B", "tierFactors": [0.5, 1.0],
                          "categories": [{"name": "Cat", "scenarios": ["Known", "Ghost", "Twin",
                                                                       {"name": "Twin", "hash": "twin-b"}]}]}})"),
                                             "spec.json", spec).isEmpty());
        ASSERT_TRUE(spec.benchmark.has_value());
        const auto benchmark = driver::generateBenchmark(driver::buildProfile(spec), *spec.benchmark);

        ASSERT_EQ(benchmark.tiers.size(), 2U);
        ASSERT_EQ(benchmark.categories.size(), 1U);
        const auto &entries = benchmark.categories[0].scenarios;
        ASSERT_EQ(entries.size(), 4U);
        EXPECT_EQ(entries[0].hash, std::optional<std::string>("spec-known"));
        EXPECT_FALSE(entries[1].hash.has_value());  // no such scenario
        EXPECT_EQ(entries[1].thresholds[1].score, 1000.0);
        EXPECT_FALSE(entries[2].hash.has_value());  // two scenarios share the name
        EXPECT_EQ(entries[3].hash, std::optional<std::string>("twin-b"));
        EXPECT_NE(entries[2].id, entries[3].id);
    }

    // ---- Modelled runs: scenario-specs and performance-specs ----

    // 1w3ts Reload's numbers from the showcase analysis: kill-paced clicking, where
    // better players mostly fire faster.
    gallery::ScenarioModel clickModel() {
        gallery::ScenarioModel model;
        model.name = "Click";
        model.hash = "click";
        model.idealShotsPerSecond = 2.31;
        model.idealAccuracy = 0.97;
        model.accuracyWeight = 0.22;
        model.shotRateWeight = 0.78;
        return model;
    }

    // VT Controlsphere's: constant fire, so skill moves accuracy only.
    gallery::ScenarioModel constantFireModel() {
        gallery::ScenarioModel model;
        model.name = "Fire";
        model.hash = "fire";
        model.firstSampleSeconds = 0.97;
        model.idealShotsPerSecond = 100.0;
        model.idealAccuracy = 0.73;
        return model;
    }

    gallery::PerformanceShape noisyShape() {
        gallery::PerformanceShape shape;
        shape.accuracyNoise = 0.3;
        shape.shotRateNoise = 0.2;
        return shape;
    }

    domain::Run modelled(const gallery::ScenarioModel &model, const double skill, const std::uint64_t seed,
                         const gallery::PerformanceShape &shape = {}) {
        return gallery::makeModelledRun({.name = model.name, .hash = model.hash}, 0, 18 * 60, model, shape, skill, 1.0,
                                        seed);
    }

    const std::vector<domain::ScenarioDataPoint> &samples(const domain::Run &run) {
        static const std::vector<domain::ScenarioDataPoint> kNone;
        return run.performance ? run.performance->samples : kNone;
    }

    TEST(DriverScenarioSpecTest, ParseErrorsNameTheField) {
        const auto error = [](const char *text) {
            gallery::ScenarioModel model;
            return driver::parseScenarioModel(json(text), model);
        };
        EXPECT_TRUE(error(R"({"name": "A", "ideal": {"shotsPerSecond": 2}})").contains("ideal.accuracy: is required"));
        EXPECT_TRUE(error(R"({"name": "A", "ideal": {"shotsPerSecond": 2, "accuracy": 1.5}})").contains("ideal.accuracy"));
        EXPECT_TRUE(error(R"({"name": "A", "ideal": {"shotsPerSecond": 2, "accuracy": 0.9}, "scoring": {"perHits": 1}})")
                        .contains("scoring.perHits"));
        EXPECT_TRUE(error(R"({"name": "A", "ideal": {"shotsPerSecond": 2, "accuracy": 0.9},
                              "skill": {"accuracy": 0, "shotRate": 0}})").contains("skill"));
        EXPECT_TRUE(error(R"({"ideal": {"shotsPerSecond": 2, "accuracy": 0.9}})").contains("name: is required"));
        EXPECT_TRUE(error(R"({"name": "A", "ideal": {"shotsPerSecond": 0, "accuracy": 0.9}})").contains("ideal.shotsPerSecond"));

        // A penalty per miss (1w3ts Pasu Micro scores 10 per hit, -2 per miss) is a valid rule.
        gallery::ScenarioModel model;
        ASSERT_EQ(driver::parseScenarioModel(json(R"({"name": "Micro", "hash": "micro", "durationSeconds": 54,
            "scoring": {"perHit": 10, "perMiss": -2}, "ideal": {"shotsPerSecond": 1.5, "accuracy": 0.9},
            "skill": {"accuracy": 0.36, "shotRate": 0.64, "maxShotsPerSecond": 2}})"), model), QString());
        EXPECT_EQ(model.hash, "micro");
        EXPECT_LT(model.scoring.perMiss, 0.0);
        EXPECT_GT(model.shotRateWeight, model.accuracyWeight);
        EXPECT_TRUE(model.maxShotsPerSecond.has_value());
    }

    TEST(DriverScenarioSpecTest, SkillOneScoresAboutWhatTheIdealRunScores) {
        for (const auto &model: {clickModel(), constantFireModel()}) {
            const double ideal = gallery::idealScore(model);
            ASSERT_GT(ideal, 0.0) << model.name;
            for (std::uint64_t seed = 0; seed < 5; ++seed) {
                EXPECT_NEAR(modelled(model, 1.0, seed, noisyShape()).totals().score, ideal, 0.03 * ideal)
                    << model.name << " seed " << seed;
                EXPECT_NEAR(modelled(model, 0.7, seed, noisyShape()).totals().score, 0.7 * ideal, 0.03 * ideal)
                    << model.name << " seed " << seed;
            }
        }
    }

    TEST(DriverScenarioSpecTest, TotalsAreTheSumOfTheSamples) {
        gallery::ScenarioModel model = clickModel();
        model.idealShotsPerSecond = 15.5;
        model.scoring.hitsPerKill = 6;
        model.scoring.perKill = 2.0;
        model.scoring.perMiss = -0.5;
        const domain::Run run = modelled(model, 0.9, 3, noisyShape());
        ASSERT_FALSE(samples(run).empty());
        domain::RunTotals summed;
        double score = 0.0;
        for (const auto &sample: samples(run)) {
            summed.shots += sample.shots;
            summed.hits += sample.hits;
            summed.misses += sample.misses;
            summed.kills += sample.kills;
            score += sample.score;
        }
        EXPECT_EQ(run.totals().shots, summed.shots);
        EXPECT_EQ(run.totals().hits, summed.hits);
        EXPECT_EQ(run.totals().misses, summed.misses);
        EXPECT_EQ(run.totals().kills, summed.kills);
        EXPECT_GT(summed.kills, 0);
        EXPECT_NEAR(run.totals().score, score, 1e-3 * std::abs(score));
    }

    TEST(DriverScenarioSpecTest, ConstantFireKeepsTheShotsAndMovesOnlyAccuracy) {
        const auto low = modelled(constantFireModel(), 0.6, 7, noisyShape());
        const auto high = modelled(constantFireModel(), 1.2, 7, noisyShape());
        ASSERT_EQ(samples(low).size(), samples(high).size());
        for (std::size_t i = 0; i < samples(low).size(); ++i) {
            EXPECT_EQ(samples(low)[i].shots, samples(high)[i].shots) << "sample " << i;
        }
        EXPECT_GT(high.totals().accuracy(), low.totals().accuracy());

        // Where skill is mostly shot rate, a better run fires more.
        EXPECT_GT(modelled(clickModel(), 1.2, 7).totals().shots, modelled(clickModel(), 0.6, 7).totals().shots);
    }

    TEST(DriverScenarioSpecTest, HighSkillIsNotClampedBelowNinetyFivePercent) {
        gallery::ScenarioModel model = constantFireModel();
        model.idealAccuracy = 0.9;
        EXPECT_GT(modelled(model, 1.1, 1).totals().accuracy(), 0.95);
    }

    TEST(DriverScenarioSpecTest, HitsNeverExceedShots) {
        for (const auto &model: {clickModel(), constantFireModel()}) {
            for (const double skill: {0.0, 0.4, 1.0, 1.6, 4.0}) {
                for (std::uint64_t seed = 0; seed < 5; ++seed) {
                    for (const auto &sample: samples(modelled(model, skill, seed, noisyShape()))) {
                        EXPECT_GE(sample.hits, 0);
                        EXPECT_LE(sample.hits, sample.shots) << model.name << " skill " << skill << " t " << sample.time;
                        EXPECT_EQ(sample.misses, sample.shots - sample.hits);
                    }
                }
            }
        }
    }

    TEST(DriverPerformanceSpecTest, ParseErrorsNameTheField) {
        const auto error = [](const char *text) {
            gallery::PerformanceShape shape;
            return driver::parsePerformanceShape(json(text), shape);
        };
        EXPECT_TRUE(error(R"({"windows": [{"from": 20, "to": 20}]})").contains("windows[0].to"));
        EXPECT_TRUE(error(R"({"windows": [{"from": 0, "to": 5}, {"to": 5}]})").contains("windows[1].from: is required"));
        EXPECT_TRUE(error(R"({"windows": [{"from": 0, "to": 5, "acuracy": 0.5}]})").contains("windows[0].acuracy"));
        EXPECT_TRUE(error(R"({"noise": {"accuracy": -0.1}})").contains("noise.accuracy"));
        EXPECT_TRUE(error(R"({"windows": [{"from": 0, "to": 5, "ramp": "yes"}]})").contains("windows[0].ramp"));

        gallery::PerformanceShape shape;
        ASSERT_EQ(driver::parsePerformanceShape(json(R"({"windows": [{"from": 50, "to": 60, "accuracy": 0.8, "ramp": true},
            {"from": 20, "to": 25, "shotRate": 0.9}], "noise": {"accuracy": 0.2}})"), shape), QString());
        ASSERT_EQ(shape.windows.size(), 2U);
        EXPECT_TRUE(shape.windows[0].ramp);
        EXPECT_GT(shape.accuracyNoise, 0.0);
    }

    double meanScoreBetween(const domain::Run &run, const double from, const double to) {
        double sum = 0.0;
        int count = 0;
        for (const auto &sample: samples(run)) {
            if (sample.time > from && sample.time <= to) {
                sum += sample.score;
                ++count;
            }
        }
        return count == 0 ? 0.0 : sum / count;
    }

    // A shape redistributes a run's score over time; the skill alone sets the total.
    TEST(DriverPerformanceSpecTest, AWindowMovesScoreOutOfItWithoutChangingTheRunsScore) {
        for (const auto &model: {constantFireModel(), clickModel()}) {
            gallery::PerformanceShape plain;
            plain.accuracyNoise = 0.1;
            gallery::PerformanceShape choke = plain;
            choke.windows.push_back({.from = 40.0, .to = 50.0, .accuracy = 0.5});
            const auto flat = modelled(model, 1.0, 11, plain);
            const auto shaped = modelled(model, 1.0, 11, choke);

            EXPECT_LT(meanScoreBetween(shaped, 40.0, 50.0), 0.8 * meanScoreBetween(flat, 40.0, 50.0)) << model.name;
            EXPECT_GT(meanScoreBetween(shaped, 0.0, 40.0), meanScoreBetween(flat, 0.0, 40.0)) << model.name;
            EXPECT_NEAR(shaped.totals().score, flat.totals().score, 0.03 * flat.totals().score) << model.name;
        }
    }

    // ---- Profile specs that reference scenario-specs and performance-specs ----

    constexpr const char *kFireSpec = R"({"name": "Fire Spec", "hash": "fire-spec",
        "ideal": {"shotsPerSecond": 100, "accuracy": 0.73}})";
    constexpr const char *kChokeSpec = R"({"windows": [{"from": 20, "to": 30, "accuracy": 0.3}]})";

    // A temporary tree of spec files; profile specs are parsed from their real path
    // so references resolve as the driver resolves them.
    struct SpecTree {
        QTemporaryDir dir;

        [[nodiscard]] QString path(const QString &relative) const { return QDir(dir.path()).filePath(relative); }

        void write(const QString &relative, const QByteArray &content) const {
            ASSERT_TRUE(writeFile(path(relative), content)) << relative.toStdString();
        }

        QString parse(const QString &relative, const QByteArray &content, driver::ProfileSpec &out) const {
            write(relative, content);
            return driver::parseProfileSpec(QJsonDocument::fromJson(content).object(), path(relative), out);
        }

        QString error(const QByteArray &content) const {
            driver::ProfileSpec spec;
            return parse("profile.json", content, spec);
        }
    };

    std::vector<domain::Run> runsInOrder(const domain::UserProfile &profile, const std::string &hash) {
        std::vector<domain::Run> runs;
        for (const auto &run: profile.getAllRunRecords())
            if (run.run_id.scenario_id.hash == hash) runs.push_back(run);
        std::ranges::sort(runs, {}, [](const domain::Run &run) { return run.run_id.start_time; });
        return runs;
    }

    double meanScore(const std::vector<domain::Run> &runs, const std::size_t from, const std::size_t to) {
        double sum = 0.0;
        for (std::size_t i = from; i < to; ++i) sum += runs[i].totals().score;
        return sum / static_cast<double>(to - from);
    }

    TEST(DriverSpecTest, ReferencedSpecsResolveRelativeToTheReferringSpec) {
        SpecTree tree;
        tree.write("scenarios/fire.json", kFireSpec);
        driver::ProfileSpec spec;
        ASSERT_EQ(tree.parse("profiles/profile.json", R"({"scenarios": [
            {"scenarioSpec": "../scenarios/fire.json", "runs": 2},
            {"scenarioSpec": "../scenarios/fire.json", "name": "Other Scenario", "runs": 1}]})", spec), QString());
        const auto profile = driver::buildProfile(spec);
        ASSERT_EQ(runsInOrder(profile, "fire-spec").size(), 2U);
        EXPECT_EQ(runsInOrder(profile, "fire-spec").front().run_id.scenario_id.name, "Fire Spec");
        // Renaming a shared scenario-spec without a hash makes a scenario of its own.
        EXPECT_EQ(runsInOrder(profile, "spec-other-scenario").size(), 1U);
    }

    TEST(DriverSpecTest, ReferenceErrorsNameTheReferringField) {
        SpecTree tree;
        tree.write("fire.json", kFireSpec);
        tree.write("choke.json", kChokeSpec);
        tree.write("bad-scenario.json", R"({"name": "Bad", "ideal": {"shotsPerSecond": 1}})");
        tree.write("bad-shape.json", R"({"windows": [{"from": 5, "to": 1}]})");
        tree.write("not-json.json", "{");

        const auto expectError = [&](const QByteArray &content, const QStringList &fragments) {
            const QString error = tree.error(content);
            for (const auto &fragment: fragments)
                EXPECT_TRUE(error.contains(fragment)) << error.toStdString() << " lacks " << fragment.toStdString();
        };
        expectError(R"({"scenarios": [{"scenarioSpec": "nope.json"}]})", {"scenarios[0].scenarioSpec", "no such file"});
        expectError(R"({"scenarios": [{"scenarioSpec": "bad-scenario.json"}]})",
                    {"scenarios[0].scenarioSpec", "ideal.accuracy: is required"});
        expectError(R"({"scenarios": [{"scenarioSpec": "not-json.json"}]})", {"scenarios[0].scenarioSpec", "JSON"});
        expectError(R"({"scenarios": [{"scenarioSpec": "fire.json", "performanceSpec": "bad-shape.json"}]})",
                    {"scenarios[0].performanceSpec", "windows[0].to"});
        expectError(R"({"scenarios": [{"scenarioSpec": "fire.json", "handRuns": [{"run": 0, "performanceSpec": "x.json"}]}]})",
                    {"scenarios[0].handRuns[0].performanceSpec", "no such file"});
        expectError(R"({"scenarios": [{"scenarioSpec": "fire.json", "runs": 3, "handRuns": [{"run": 3}]}]})",
                    {"scenarios[0].handRuns[0].run"});
        expectError(R"({"scenarios": [{"scenarioSpec": "fire.json", "handRuns": [{"run": 1}, {"run": 1}]}]})",
                    {"scenarios[0].handRuns[1].run"});
        expectError(R"({"scenarios": [{"scenarioSpec": "fire.json", "shape": "gallery-pasu"}]})", {"scenarios[0].shape"});
        expectError(R"({"scenarios": [{"name": "Legacy", "idealScore": 100}]})", {"scenarios[0].idealScore"});
        expectError(R"({"scenarios": [{"scenarioSpec": "fire.json", "skill": 1,
                                       "progression": {"keyframes": [{"at": 0, "skill": 1}]}}]})", {"scenarios[0]"});
        expectError(R"({"scenarios": [{"scenarioSpec": "fire.json", "progression": {"keyframes": []}}]})",
                    {"scenarios[0].progression.keyframes"});
        expectError(R"({"scenarios": [{"scenarioSpec": "fire.json", "progression": {"by": "week",
                                       "keyframes": [{"at": 0, "skill": 1}]}}]})", {"scenarios[0].progression.by"});
        expectError(R"({"scenarios": [{"scenarioSpec": "fire.json", "progression":
                                       {"keyframes": [{"at": 5, "skill": 1}, {"at": 5, "skill": 2}]}}]})",
                    {"scenarios[0].progression.keyframes[1].at"});
    }

    TEST(DriverSpecTest, IdealScoreRescalesTheScenarioSpecSoSkillOneLandsOnIt) {
        SpecTree tree;
        tree.write("fire.json", kFireSpec);
        driver::ProfileSpec spec;
        ASSERT_EQ(tree.parse("profile.json", R"({"scenarios": [
            {"scenarioSpec": "fire.json", "hash": "at-ideal", "runs": 3, "idealScore": 500},
            {"scenarioSpec": "fire.json", "hash": "below-ideal", "runs": 3, "idealScore": 500, "skill": 0.8}]})", spec),
                  QString());
        const auto profile = driver::buildProfile(spec);
        for (const auto &run: runsInOrder(profile, "at-ideal")) EXPECT_NEAR(run.totals().score, 500.0, 15.0);
        for (const auto &run: runsInOrder(profile, "below-ideal")) EXPECT_NEAR(run.totals().score, 400.0, 15.0);
    }

    TEST(DriverSpecTest, RisingKeyframesLiftLaterRunsAndRunsStillFluctuate) {
        SpecTree tree;
        tree.write("fire.json", kFireSpec);
        driver::ProfileSpec spec;
        ASSERT_EQ(tree.parse("profile.json", R"({"scenarios": [
            {"scenarioSpec": "fire.json", "hash": "by-run", "runs": 40, "runsPerDay": 2,
             "progression": {"keyframes": [{"at": 0, "skill": 0.5}, {"at": 39, "skill": 1.0}],
                             "fluctuation": 0.08, "dayFluctuation": 0.04}},
            {"scenarioSpec": "fire.json", "hash": "by-day", "runs": 40, "runsPerDay": 2, "firstDay": 3,
             "progression": {"by": "day", "keyframes": [{"at": 3, "skill": 0.5}, {"at": 22, "skill": 1.0}],
                             "fluctuation": 0.08}}]})", spec), QString());
        const auto profile = driver::buildProfile(spec);
        for (const std::string hash: {"by-run", "by-day"}) {
            const auto runs = runsInOrder(profile, hash);
            ASSERT_EQ(runs.size(), 40U);
            EXPECT_GT(meanScore(runs, 30, 40), 1.5 * meanScore(runs, 0, 10)) << hash;
            int drops = 0;
            for (std::size_t i = 1; i < runs.size(); ++i) drops += runs[i].totals().score < runs[i - 1].totals().score;
            EXPECT_GT(drops, 3) << hash;
        }
    }

    TEST(DriverSpecTest, HandRunsKeepTheirOwnSkillAndShape) {
        SpecTree tree;
        tree.write("fire.json", kFireSpec);
        tree.write("choke.json", kChokeSpec);
        driver::ProfileSpec spec;
        ASSERT_EQ(tree.parse("profile.json", R"({"scenarios": [
            {"scenarioSpec": "fire.json", "runs": 10, "idealScore": 1000,
             "progression": {"keyframes": [{"at": 0, "skill": 0.6}], "fluctuation": 0.2},
             "handRuns": [{"run": 4, "skill": 1.2, "performanceSpec": "choke.json"},
                          {"run": 6, "performanceSpec": "choke.json"}]}]})", spec), QString());
        const auto runs = runsInOrder(driver::buildProfile(spec), "fire-spec");
        ASSERT_EQ(runs.size(), 10U);

        EXPECT_NEAR(runs[4].totals().score, 1200.0, 30.0);
        EXPECT_NEAR(runs[6].totals().score, 600.0, 20.0);  // the trend, without fluctuation
        for (const std::size_t hand: {4U, 6U}) {
            EXPECT_LT(meanScoreBetween(runs[hand], 20.0, 30.0), 0.6 * meanScoreBetween(runs[hand], 0.0, 20.0)) << hand;
        }
        EXPECT_GT(meanScoreBetween(runs[5], 20.0, 30.0), 0.8 * meanScoreBetween(runs[5], 0.0, 20.0));
    }

    TEST(DriverSpecTest, PerformanceFreeModelledRunsKeepOnlyTheirTotals) {
        SpecTree tree;
        tree.write("fire.json", kFireSpec);
        driver::ProfileSpec spec;
        ASSERT_EQ(tree.parse("profile.json", R"({"scenarios": [{"scenarioSpec": "fire.json", "runs": 2,
                                                                "performance": false}]})", spec), QString());
        for (const auto &run: driver::buildProfile(spec).getAllRunRecords()) {
            EXPECT_FALSE(run.performance.has_value());
            EXPECT_GT(run.stored_totals.score, 0.0F);
        }
    }

    TEST(DriverSpecTest, ModelledSpecsWriteTheSameBytesEveryTime) {
        SpecTree tree;
        tree.write("scenarios/fire.json", kFireSpec);
        tree.write("performances/choke.json", kChokeSpec);
        tree.write("profile.json", R"({"scenarios": [{"scenarioSpec": "scenarios/fire.json",
            "performanceSpec": "performances/choke.json", "runs": 12, "runsPerDay": 3,
            "progression": {"by": "day", "keyframes": [{"at": 0, "skill": 0.7}, {"at": 3, "skill": 0.9}],
                            "fluctuation": 0.1, "dayFluctuation": 0.05},
            "handRuns": [{"run": 11, "skill": 1.1}]}]})");
        const QString benchmarks = tree.path("benchmarks");
        ASSERT_EQ(driver::writeProfileSpec(tree.path("profile.json"), tree.path("first.pb"), benchmarks), QString());
        ASSERT_EQ(driver::writeProfileSpec(tree.path("profile.json"), tree.path("second.pb"), benchmarks), QString());
        EXPECT_FALSE(readFile(tree.path("first.pb")).isEmpty());
        EXPECT_EQ(readFile(tree.path("first.pb")), readFile(tree.path("second.pb")));
    }

    TEST(DriverSpecTest, CheckedInSpecsBuild) {
        const QDir driverDir(QStringLiteral(KSV_DRIVER_BENCHMARKS_DIR "/.."));
        QStringList profileSpecs{driverDir.filePath("tours/composing-example/twins.json")};
        const QDir specs(driverDir.filePath("specs"));
        for (const auto &file: specs.entryList({"*.json"}, QDir::Files, QDir::Name)) profileSpecs << specs.filePath(file);
        ASSERT_GE(profileSpecs.size(), 2);
        for (const auto &specPath: profileSpecs) {
            QTemporaryDir out;
            EXPECT_EQ(driver::writeProfileSpec(specPath, QDir(out.path()).filePath("profile.pb"),
                                               QDir(out.path()).filePath("benchmarks")), QString());
        }
        for (const auto &file: QDir(specs.filePath("scenarios")).entryList({"*.json"}, QDir::Files)) {
            gallery::ScenarioModel model;
            EXPECT_EQ(driver::parseScenarioModel(QJsonDocument::fromJson(readFile(specs.filePath("scenarios/" + file))).object(),
                                                 model), QString()) << file.toStdString();
        }
        for (const auto &file: QDir(specs.filePath("performances")).entryList({"*.json"}, QDir::Files)) {
            gallery::PerformanceShape shape;
            EXPECT_EQ(driver::parsePerformanceShape(
                          QJsonDocument::fromJson(readFile(specs.filePath("performances/" + file))).object(), shape),
                      QString()) << file.toStdString();
        }
    }

    // Real runs start with a partial first second, and Polarized Hell's 62.4 s
    // length ends on a 0.4 s sample. Here: 0.6 s first, then whole seconds, then 0.4 s.
    TEST(DriverScenarioSpecTest, PartialSecondsCarryProportionallyFewerShots) {
        gallery::ScenarioModel model = constantFireModel();
        model.durationSeconds = 62.0;
        model.firstSampleSeconds = 0.6;
        const auto run = modelled(model, 1.0, 2);
        ASSERT_GE(samples(run).size(), 3U);
        const int fullSecond = samples(run)[1].shots;
        EXPECT_LT(samples(run).front().shots, fullSecond * 3 / 4);
        EXPECT_LT(samples(run).back().shots, fullSecond * 3 / 4);
        EXPECT_FLOAT_EQ(samples(run).back().time, static_cast<float>(model.durationSeconds));
        EXPECT_FLOAT_EQ(run.scenario_length, static_cast<float>(model.durationSeconds));
    }

    // The benchmark fixtures (tools/driver/benchmarks) have fixed thresholds picked
    // against the rich profile's scores so that each row shows a different state.
    // These tests fail when the run generator or the rich profile drifts from them;
    // re-pick the thresholds rather than loosening the expectations.
    struct SeededProjection {
        domain::Benchmark benchmark;
        domain::BenchmarkProjection projection;

        [[nodiscard]] const domain::ScenarioProjection &scenario(const std::string &entryId) const {
            const auto found = std::ranges::find_if(projection.scenarios, [&](const domain::ScenarioProjection &row) {
                return row.entryId.value == entryId;
            });
            if (found == projection.scenarios.end()) throw std::runtime_error("no row " + entryId);
            return *found;
        }

        [[nodiscard]] const domain::ScenarioEntry &entry(const std::string &entryId) const {
            for (const auto &category: benchmark.categories) {
                for (const auto &candidate: category.scenarios)
                    if (candidate.id.value == entryId) return candidate;
                for (const auto &subcategory: category.subcategories)
                    for (const auto &candidate: subcategory.scenarios)
                        if (candidate.id.value == entryId) return candidate;
            }
            throw std::runtime_error("no entry " + entryId);
        }

        // How many tiers a score reaches on an entry's ladder (0: below the first).
        [[nodiscard]] int tiersReached(const std::string &entryId, const std::optional<double> score) const {
            return static_cast<int>(std::floor(domain::normalizedTierValue(benchmark.tiers, entry(entryId).thresholds, score)));
        }

        [[nodiscard]] std::optional<std::string> attained(const std::string &entryId) const {
            const auto &tier = scenario(entryId).attainedTier;
            return tier ? std::optional(tier->value) : std::nullopt;
        }

        [[nodiscard]] std::optional<std::string> groupTier(const std::string &groupId) const {
            for (const auto &group: projection.categories) {
                if (group.id.value == groupId) return group.satisfiedTier ? std::optional(group.satisfiedTier->value) : std::nullopt;
                for (const auto &subgroup: group.subgroups)
                    if (subgroup.id.value == groupId)
                        return subgroup.satisfiedTier ? std::optional(subgroup.satisfiedTier->value) : std::nullopt;
            }
            throw std::runtime_error("no group " + groupId);
        }
    };

    // Seeds a `dataset` session as ksv_driver does and evaluates one of its benchmark files
    // against the seeded profile, every hashed entry resolved as the app resolves it.
    SeededProjection projectSeededBenchmark(const QString &dataset, const std::string &filename) {
        QTemporaryDir dir;
        const driver::SessionData data{dir.path()};
        const QString error = driver::seedSessionData(data, dataset, {}, QStringLiteral(KSV_DRIVER_BENCHMARKS_DIR));
        if (!error.isEmpty()) throw std::runtime_error(error.toStdString());
        const auto loadResult = data::ProfileSerializer().load(data.profilePath().toStdString());
        const auto *profile = std::get_if<domain::UserProfile>(&loadResult);
        if (!profile) throw std::runtime_error("seeded profile did not load");

        const auto scan = qt_data::BenchmarkStore(data.benchmarksDir().toStdString()).scan();
        if (!scan.snapshot) throw std::runtime_error("benchmark scan failed");
        const auto file = std::ranges::find_if(scan.snapshot->entries, [&](const data::BenchmarkFileEntry &candidate) {
            return candidate.filename == filename;
        });
        if (file == scan.snapshot->entries.end()) throw std::runtime_error("no benchmark file " + filename);
        const auto &loaded = std::get<data::LoadedBenchmark>(file->content);

        std::set<std::string> played;
        for (const auto &scenario: profile->getScenarioList()) played.insert(scenario.hash);
        std::vector<domain::ScenarioResolution> resolutions;
        std::vector<domain::ScenarioId> scenarios;
        const auto resolve = [&](const domain::ScenarioEntry &entry) {
            const bool found = entry.hash && played.contains(*entry.hash);
            resolutions.push_back({entry.id, found ? domain::ScenarioMatchState::Resolved
                                                   : domain::ScenarioMatchState::MappedUnavailable,
                                   entry.hash, {}});
            if (found) scenarios.push_back({{}, *entry.hash});
        };
        for (const auto &category: loaded.benchmark.categories) {
            std::ranges::for_each(category.scenarios, resolve);
            for (const auto &subcategory: category.subcategories) std::ranges::for_each(subcategory.scenarios, resolve);
        }
        return {loaded.benchmark,
                domain::evaluateBenchmark(loaded.benchmark, loaded.completeness, resolutions,
                                          profile->getRunFacts(scenarios), {})};
    }

    TEST(DriverBenchmarkFixtureTest, DemoBenchmarkSpreadsTheRichProfileAcrossEveryRowState) {
        const auto seeded = projectSeededBenchmark("rich", "driver-demo.json");
        const auto &projection = seeded.projection;
        ASSERT_EQ(projection.completeness, domain::Completeness::Trackable);
        EXPECT_EQ(seeded.benchmark.id.value, driver::kDefaultBenchmarkId);

        // Above the top tier, with its recent average back inside the Platinum chunk.
        EXPECT_EQ(seeded.attained("entry-gallery-primary"), "platinum");
        EXPECT_GT(*seeded.scenario("entry-gallery-primary").personalBest,
                  seeded.entry("entry-gallery-primary").thresholds.back().score);
        EXPECT_EQ(seeded.tiersReached("entry-gallery-primary", seeded.scenario("entry-gallery-primary").recentAverage), 4);
        // Just into Bronze, its recent average still in Iron.
        EXPECT_EQ(seeded.attained("entry-gallery-pasu"), "bronze");
        EXPECT_EQ(seeded.tiersReached("entry-gallery-pasu", seeded.scenario("entry-gallery-pasu").recentAverage), 1);
        // Gold, nine tenths of the way to Platinum.
        EXPECT_EQ(seeded.attained("entry-gallery-smoothbot"), "gold");
        EXPECT_NEAR(seeded.scenario("entry-gallery-smoothbot").normalizedRank, 4.9, 0.001);
        // Iron, its recent average below it.
        EXPECT_EQ(seeded.attained("entry-gallery-flyts"), "iron");
        EXPECT_EQ(seeded.tiersReached("entry-gallery-flyts", seeded.scenario("entry-gallery-flyts").recentAverage), 0);
        // Played but below Iron, under the long real scenario name.
        EXPECT_EQ(seeded.attained("entry-gallery-celestial"), std::nullopt);
        EXPECT_TRUE(seeded.scenario("entry-gallery-celestial").personalBest.has_value());
        EXPECT_EQ(seeded.scenario("entry-gallery-celestial").name, "Air CELESTIAL No UFO Easy Slowed");

        // Clicking ranks by its weaker subcategory, Tracking by its best scenario.
        EXPECT_EQ(seeded.groupTier("static"), "platinum");
        EXPECT_EQ(seeded.groupTier("dynamic"), "bronze");
        EXPECT_EQ(seeded.groupTier("clicking"), "bronze");
        EXPECT_EQ(seeded.groupTier("tracking"), "gold");
        ASSERT_TRUE(projection.attainedRank);
        EXPECT_EQ(projection.attainedRank->value, "bronze");
        EXPECT_FALSE(projection.completedRank.has_value());
        ASSERT_TRUE(projection.nextTier);
        EXPECT_EQ(projection.nextTier->value, "silver");
        ASSERT_EQ(projection.nextTierBlockers.size(), 1U);
        EXPECT_EQ(projection.nextTierBlockers[0].entryId.value, "entry-gallery-pasu");
        EXPECT_EQ(projection.scenariosAtNextTier, 2);

        // The personal-best average climbs in several steps over the month.
        ASSERT_GE(projection.averageRankHistory.size(), 5U);
        EXPECT_GT(projection.averageRankHistory.back().averageRank - projection.averageRankHistory.front().averageRank, 0.5);

        // The recent-average line has a point on every play day, at least as many as the PB line,
        // and ends at the mean of each scenario's current recent average.
        ASSERT_GE(projection.recentAverageRankHistory.size(), projection.averageRankHistory.size());
        double recentMean = 0.0;
        for (const auto &row: projection.scenarios)
            recentMean += domain::normalizedTierValue(seeded.benchmark.tiers, seeded.entry(row.entryId.value).thresholds,
                                                      row.recentAverage);
        recentMean /= static_cast<double>(projection.scenarios.size());
        EXPECT_NEAR(projection.recentAverageRankHistory.back().averageRank, recentMean, 1e-9);
    }

    TEST(DriverBenchmarkFixtureTest, ManyTiersBenchmarkRanksEveryRowOnATwelveTierLadder) {
        const auto seeded = projectSeededBenchmark("rich", "driver-many-tiers.json");
        const auto &projection = seeded.projection;
        ASSERT_EQ(projection.completeness, domain::Completeness::Trackable);
        EXPECT_EQ(seeded.benchmark.tiers.size(), 12U);

        EXPECT_EQ(seeded.attained("entry-gallery-primary"), "grandmaster");
        EXPECT_EQ(seeded.attained("entry-gallery-pasu"), "gold");
        EXPECT_EQ(seeded.attained("entry-gallery-smoothbot"), "nova");
        EXPECT_EQ(seeded.attained("entry-gallery-flyts"), "platinum");
        EXPECT_EQ(seeded.attained("entry-gallery-celestial"), "iron");

        ASSERT_TRUE(projection.attainedRank);
        EXPECT_EQ(projection.attainedRank->value, "grandmaster");
        ASSERT_TRUE(projection.completedRank);
        EXPECT_EQ(projection.completedRank->value, "iron");
        ASSERT_TRUE(projection.nextTier);
        EXPECT_EQ(projection.nextTier->value, "nova");
        std::set<std::string> blockers;
        for (const auto &blocker: projection.nextTierBlockers) blockers.insert(blocker.entryId.value);
        EXPECT_EQ(blockers, (std::set<std::string>{"entry-gallery-primary", "entry-gallery-pasu"}));
    }

    TEST(DriverBenchmarkFixtureTest, SingleScenarioDatasetsLeaveTheOtherRowsUnplayed) {
        const auto seeded = projectSeededBenchmark("single", "driver-demo.json");
        EXPECT_EQ(seeded.attained("entry-gallery-primary"), "silver");
        for (const auto *entryId: {"entry-gallery-pasu", "entry-gallery-smoothbot", "entry-gallery-flyts",
                                   "entry-gallery-celestial"}) {
            EXPECT_EQ(seeded.scenario(entryId).matchState, domain::ScenarioMatchState::MappedUnavailable) << entryId;
        }
        EXPECT_FALSE(seeded.projection.attainedRank.has_value());
    }

    TEST(DriverFaultTest, ParsesPortsOpsAndEffectsAndNamesBadInput) {
        driver::FaultPlan plan;
        EXPECT_TRUE(plan.add("benchmarkStore.write=fail").isEmpty());
        EXPECT_TRUE(plan.add("decoder=delay:5,fail:2").isEmpty());
        EXPECT_EQ(plan.describe(), (QStringList{"benchmarkStore.write=fail", "decoder=delay:5,fail:2"}));

        EXPECT_TRUE(plan.add("disk=fail").contains("unknown port 'disk'"));
        EXPECT_TRUE(plan.add("disk=fail").contains("seriesStore (load, save)"));
        EXPECT_TRUE(plan.add("csv.write=fail").contains("no op 'write'"));
        EXPECT_TRUE(plan.add("csv=explode").contains("unknown effect 'explode'"));
        EXPECT_TRUE(plan.add("csv=fail:0").contains("unknown effect"));
        EXPECT_TRUE(plan.add("csv").contains("PORT[.OP]=EFFECT"));
    }

    TEST(DriverFaultTest, CountsCallsForNthAndAfterEffects) {
        driver::FaultPlan plan;
        ASSERT_TRUE(plan.add("csv=fail:2").isEmpty());
        ASSERT_TRUE(plan.add("benchmarkStore.scan=fail:after:1").isEmpty());

        EXPECT_FALSE(plan.beforeCall("csv", "parseFile"));
        EXPECT_TRUE(plan.beforeCall("csv", "parseFile"));
        EXPECT_FALSE(plan.beforeCall("csv", "parseFile"));
        EXPECT_FALSE(plan.beforeCall("benchmarkStore", "scan"));
        EXPECT_TRUE(plan.beforeCall("benchmarkStore", "scan"));
        EXPECT_TRUE(plan.beforeCall("benchmarkStore", "scan"));
        EXPECT_FALSE(plan.beforeCall("benchmarkStore", "write"));
    }

    TEST(DriverFaultTest, DecoratorsReturnEachPortsNaturalFailure) {
        auto plan = std::make_shared<driver::FaultPlan>();
        ASSERT_TRUE(plan->add("benchmarkStore=fail").isEmpty());
        ASSERT_TRUE(plan->add("seriesStore=fail").isEmpty());
        ASSERT_TRUE(plan->add("decoder.decode_file=delay:60,fail").isEmpty());

        QTemporaryDir dir;
        const auto store = driver::injectBenchmarkStore(
            std::make_shared<qt_data::BenchmarkStore>(dir.path().toStdString()), plan);
        EXPECT_EQ(store->scan().failure, data::BenchmarkScanFailure::DirectoryUnavailable);
        EXPECT_EQ(store->write({}, "x.json", std::nullopt).failure, data::BenchmarkWriteFailure::WriteFailed);
        EXPECT_FALSE(store->remove("x.json", "digest").ok);
        EXPECT_EQ(store->managedDirectoryPath(), dir.path().toStdString());

        const auto settings = std::make_shared<tests_support::FakeSettingsService>();
        settings->document = "{}";
        const auto graphs = driver::injectGraphConfigurationPersistence(
            std::make_shared<qt_data::GraphConfigurationPersistence>(settings), plan);
        auto configuration = graphs->load();
        EXPECT_EQ(configuration, application::defaultGraphConfiguration());
        configuration.runAxisLabel = application::SeriesId{2};
        graphs->save(configuration);
        EXPECT_EQ(settings->document, "{}");
        EXPECT_TRUE(settings->quarantined.empty());

        const auto decoder = driver::injectDecoder(std::make_shared<data::ProtoDecoder>(), plan);
        QElapsedTimer timer;
        timer.start();
        EXPECT_THROW(decoder->decode_file("whatever.perf"), std::runtime_error);
        EXPECT_GE(timer.elapsed(), 55);

        // Ports the plan does not name are handed through undecorated.
        const auto parser = std::make_shared<data::StatsCsvParser>();
        EXPECT_EQ(driver::injectCsvParser(parser, plan), parser);
    }


    TEST(DriverOverlayTest, CopiesTheTreeReplacingFilesAndCreatingFolders) {
        QTemporaryDir overlay;
        QTemporaryDir data;
        ASSERT_TRUE(overlay.isValid() && data.isValid());
        ASSERT_TRUE(writeFile(QDir(overlay.path()).filePath("benchmarks/broken.json"), "{"));
        ASSERT_TRUE(writeFile(QDir(overlay.path()).filePath("FPSAimTrainer/stats/a.csv"), "new"));
        ASSERT_TRUE(writeFile(QDir(data.path()).filePath("FPSAimTrainer/stats/a.csv"), "old"));

        EXPECT_TRUE(driver::applyOverlay(overlay.path(), data.path()).isEmpty());

        EXPECT_EQ(readFile(QDir(data.path()).filePath("benchmarks/broken.json")), "{");
        EXPECT_EQ(readFile(QDir(data.path()).filePath("FPSAimTrainer/stats/a.csv")), "new");
    }

    TEST(DriverOverlayTest, AMissingOverlayIsNamedInTheError) {
        QTemporaryDir data;
        const QString error = driver::applyOverlay(QDir(data.path()).filePath("no-such-overlay"), data.path());

        EXPECT_TRUE(error.contains("no-such-overlay")) << error.toStdString();
    }

    TEST(DriverSettingTest, ParsesJsonValuesStringsAndRemovals) {
        driver::SettingAssignment assignment;

        ASSERT_TRUE(driver::parseSettingAssignment("view/scenarioGraphVisible=false", assignment).isEmpty());
        EXPECT_EQ(assignment.key, "view/scenarioGraphVisible");
        EXPECT_EQ(assignment.value->typeId(), QMetaType::Bool);

        ASSERT_TRUE(driver::parseSettingAssignment("benchmarks/someId=driver-demo", assignment).isEmpty());
        EXPECT_EQ(assignment.value->toString(), "driver-demo");

        ASSERT_TRUE(driver::parseSettingAssignment("a/list=[\"x\", \"y\"]", assignment).isEmpty());
        EXPECT_EQ(assignment.value->toStringList(), (QStringList{"x", "y"}));

        ASSERT_TRUE(driver::parseSettingAssignment("file/kovaaksDirs=", assignment).isEmpty());
        EXPECT_FALSE(assignment.value.has_value());

        EXPECT_TRUE(driver::parseSettingAssignment("no-equals", assignment).contains("no-equals"));
        EXPECT_TRUE(driver::parseSettingAssignment("=1", assignment).contains("empty key"));
    }

    TEST(DriverSettingTest, UrlKeysAreStoredTheWaySettingsServiceStoresThem) {
        qt_data::SettingsService service(QSettings::IniFormat);
        const auto apply = [&](const QString &text) {
            driver::SettingAssignment assignment;
            EXPECT_TRUE(driver::parseSettingAssignment(text, assignment).isEmpty());
            return driver::applySetting(assignment, service);
        };

        ASSERT_TRUE(apply("file/kovaaksDirs=[\"D:/Games/One\", \"E:/Games/Two\"]").isEmpty());
        EXPECT_EQ(service.getKovaaksDirs(), (std::vector<std::string>{"D:/Games/One", "E:/Games/Two"}));
        ASSERT_TRUE(apply("file/kovaaksDirs=D:/Games/Solo").isEmpty());
        EXPECT_EQ(service.getKovaaksDirs(), (std::vector<std::string>{"D:/Games/Solo"}));
        ASSERT_TRUE(apply("file/profilePath=D:/Sandbox/profile.pb").isEmpty());
        EXPECT_EQ(service.getProfilePath(), "D:/Sandbox/profile.pb");

        service.setFallbackKovaaksDir("D:/Sandbox/fallback");
        ASSERT_TRUE(apply("file/kovaaksDirs=").isEmpty());
        EXPECT_EQ(service.getKovaaksDirs(), (std::vector<std::string>{"D:/Sandbox/fallback"}));

        EXPECT_FALSE(apply("file/kovaaksDirs=42").isEmpty());
        EXPECT_FALSE(apply("file/profilePath=[1]").isEmpty());
    }

    TEST(DriverSettingTest, OtherKeysAreWrittenWithTheirJsonType) {
        qt_data::SettingsService service(QSettings::IniFormat);
        driver::SettingAssignment assignment;
        ASSERT_TRUE(driver::parseSettingAssignment("driverSettingTest/visible=false", assignment).isEmpty());
        ASSERT_TRUE(driver::applySetting(assignment, service).isEmpty());
        ASSERT_TRUE(driver::parseSettingAssignment("benchmarks/recentIds=[\"spec-bench\", \"other\"]", assignment)
            .isEmpty());
        ASSERT_TRUE(driver::applySetting(assignment, service).isEmpty());

        const QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
        EXPECT_FALSE(settings.value("driverSettingTest/visible", true).toBool());
        EXPECT_EQ(service.getRecentBenchmarkIds(), (std::vector<std::string>{"spec-bench", "other"}));

        ASSERT_TRUE(driver::parseSettingAssignment("benchmarks/recentIds=", assignment).isEmpty());
        ASSERT_TRUE(driver::applySetting(assignment, service).isEmpty());
        EXPECT_TRUE(service.getRecentBenchmarkIds().empty());
    }
}
