#include "driver_spec.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <set>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>

#include "formats/protobuf/profile_serializer.h"
#include "formats/protobuf/schema/profile.pb.h"
#include "qt_data/benchmark_store.h"

namespace ksv::driver {
    namespace {
        // 2026-08-01 00:00 UTC, the gallery's day 0.
        constexpr long long kSpecEpochSeconds = 1785542400;

        // Thrown inside the parser; parseProfileSpec turns it into its error string.
        struct SpecError {
            QString message;
        };

        [[noreturn]] void reject(const QString &path, const QString &problem) {
            throw SpecError{QStringLiteral("%1: %2").arg(path, problem)};
        }

        // Rejects keys the schema does not know, naming the first by its path.
        void requireKnownKeys(const QJsonObject &object, const QString &path, const QStringList &known) {
            for (auto it = object.begin(); it != object.end(); ++it) {
                if (!known.contains(it.key())) {
                    reject(path.isEmpty() ? it.key() : path + u'.' + it.key(),
                           QStringLiteral("unknown field (expected one of: %1)").arg(known.join(QStringLiteral(", "))));
                }
            }
        }

        QString fieldPath(const QString &parent, const QString &key) {
            return parent.isEmpty() ? key : parent + u'.' + key;
        }

        int intField(const QJsonObject &object, const QString &parent, const QString &key, const int fallback,
                     const int minimum, const int maximum = std::numeric_limits<int>::max()) {
            if (!object.contains(key)) return fallback;
            const QJsonValue value = object.value(key);
            const double number = value.toDouble(std::nan(""));
            if (!value.isDouble() || number != std::floor(number) || number < minimum || number > maximum) {
                reject(fieldPath(parent, key), maximum == std::numeric_limits<int>::max()
                                                   ? QStringLiteral("expected an integer >= %1").arg(minimum)
                                                   : QStringLiteral("expected an integer from %1 to %2").arg(minimum).arg(maximum));
            }
            return static_cast<int>(number);
        }

        double numberField(const QJsonObject &object, const QString &parent, const QString &key, const double fallback,
                           const double minimum) {
            if (!object.contains(key)) return fallback;
            const QJsonValue value = object.value(key);
            if (!value.isDouble() || value.toDouble() < minimum) {
                reject(fieldPath(parent, key), QStringLiteral("expected a number >= %1").arg(minimum));
            }
            return value.toDouble();
        }

        // A finite number in [minimum, maximum] (or (minimum, maximum] when exclusive);
        // without a fallback the field is required.
        double boundedNumber(const QJsonObject &object, const QString &parent, const QString &key,
                             const std::optional<double> fallback, const double minimum,
                             const double maximum = std::numeric_limits<double>::infinity(),
                             const bool exclusiveMinimum = false) {
            if (!object.contains(key)) {
                if (!fallback) reject(fieldPath(parent, key), QStringLiteral("is required"));
                return *fallback;
            }
            const QJsonValue value = object.value(key);
            const double number = value.toDouble();
            const bool below = exclusiveMinimum ? number <= minimum : number < minimum;
            if (!value.isDouble() || !std::isfinite(number) || below || number > maximum) {
                const QString lower = QStringLiteral("%1 %2").arg(exclusiveMinimum ? ">" : ">=").arg(minimum);
                QString expected = QStringLiteral("expected a number");
                if (std::isfinite(minimum) && std::isfinite(maximum)) expected += QStringLiteral(" %1 and <= %2").arg(lower).arg(maximum);
                else if (std::isfinite(minimum)) expected += u' ' + lower;
                reject(fieldPath(parent, key), expected);
            }
            return number;
        }

        QString stringField(const QJsonObject &object, const QString &parent, const QString &key, const QString &fallback,
                            const bool required = false) {
            if (!object.contains(key)) {
                if (required) reject(fieldPath(parent, key), QStringLiteral("is required"));
                return fallback;
            }
            const QJsonValue value = object.value(key);
            if (!value.isString() || value.toString().isEmpty()) {
                reject(fieldPath(parent, key), QStringLiteral("expected a non-empty string"));
            }
            return value.toString();
        }

        bool boolField(const QJsonObject &object, const QString &parent, const QString &key, const bool fallback) {
            if (!object.contains(key)) return fallback;
            if (!object.value(key).isBool()) reject(fieldPath(parent, key), QStringLiteral("expected true or false"));
            return object.value(key).toBool();
        }

        QJsonObject objectAt(const QJsonValue &value, const QString &path) {
            if (!value.isObject()) reject(path, QStringLiteral("expected an object"));
            return value.toObject();
        }

        QJsonArray arrayField(const QJsonObject &object, const QString &parent, const QString &key) {
            if (!object.contains(key)) return {};
            if (!object.value(key).isArray()) reject(fieldPath(parent, key), QStringLiteral("expected an array"));
            return object.value(key).toArray();
        }

        domain::BenchmarkColor colorField(const QJsonObject &object, const QString &parent, const QString &key,
                                          const domain::BenchmarkColor fallback) {
            if (!object.contains(key)) return fallback;
            const QColor color(object.value(key).toString());
            if (!object.value(key).isString() || !color.isValid()) {
                reject(fieldPath(parent, key), QStringLiteral("expected a colour such as \"#00bcd4\""));
            }
            return {static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
                    static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())};
        }

        std::string slug(const QString &text) {
            static const QRegularExpression separators(QStringLiteral("[^a-z0-9]+"));
            return text.toLower().replace(separators, QStringLiteral("-")).remove(QRegularExpression("^-|-$")).toStdString();
        }

        gallery::RunShape parseShape(const QJsonValue &value, const QString &path) {
            static const QStringList kNamed{"gallery-primary", "gallery-pasu", "gallery-smoothbot", "gallery-flyts",
                                            "gallery-celestial"};
            if (value.isString()) {
                if (!kNamed.contains(value.toString())) {
                    reject(path, QStringLiteral("unknown shape '%1' (expected %2, or an object of base values)")
                                     .arg(value.toString(), kNamed.join(QStringLiteral(", "))));
                }
                return gallery::shapeForHash(value.toString().toStdString());
            }
            const QJsonObject object = objectAt(value, path);
            requireKnownKeys(object, path, {"shotsPerSecond", "shotVariation", "accuracy", "hitsPerKill",
                                            "damagePerHit", "scorePerHit", "scorePerKill"});
            gallery::RunShape shape = gallery::shapeForHash("gallery-primary");
            shape.base_shots_per_second = intField(object, path, "shotsPerSecond", shape.base_shots_per_second, 1);
            shape.shot_variation = intField(object, path, "shotVariation", shape.shot_variation, 1);
            shape.base_accuracy = static_cast<float>(numberField(object, path, "accuracy", shape.base_accuracy, 0.0));
            if (shape.base_accuracy > 1.0F) reject(fieldPath(path, "accuracy"), QStringLiteral("expected 0 to 1"));
            shape.hits_per_kill = intField(object, path, "hitsPerKill", shape.hits_per_kill, 1);
            shape.damage_per_hit = static_cast<float>(numberField(object, path, "damagePerHit", shape.damage_per_hit, 0.0));
            shape.score_per_hit = static_cast<float>(numberField(object, path, "scorePerHit", shape.score_per_hit, 0.0));
            shape.score_per_kill = static_cast<float>(numberField(object, path, "scorePerKill", shape.score_per_kill, 0.0));
            return shape;
        }

        // ---- scenario-specs and performance-specs ----

        QJsonObject optionalObject(const QJsonObject &object, const QString &parent, const QString &key) {
            return object.contains(key) ? objectAt(object.value(key), fieldPath(parent, key)) : QJsonObject{};
        }

        gallery::ScenarioModel parseModelObject(const QJsonObject &object) {
            constexpr double kInfinity = std::numeric_limits<double>::infinity();
            requireKnownKeys(object, {}, {"name", "hash", "description", "durationSeconds", "firstSampleSeconds",
                                          "scoring", "ideal", "skill"});
            gallery::ScenarioModel model;
            const QString name = stringField(object, {}, "name", {}, true);
            model.name = name.toStdString();
            model.hash = stringField(object, {}, "hash", QString::fromStdString("spec-" + slug(name))).toStdString();
            if (object.contains("description")) stringField(object, {}, "description", {});
            model.durationSeconds = boundedNumber(object, {}, "durationSeconds", model.durationSeconds, 1.0);
            model.firstSampleSeconds = boundedNumber(object, {}, "firstSampleSeconds", model.firstSampleSeconds, 0.0,
                                                     kInfinity, true);

            const QString scoringPath = QStringLiteral("scoring");
            const QJsonObject scoring = optionalObject(object, {}, scoringPath);
            requireKnownKeys(scoring, scoringPath, {"perHit", "perKill", "perMiss", "perDamage", "hitsPerKill",
                                                    "damagePerHit"});
            gallery::ScoringRules &rules = model.scoring;
            rules.perHit = boundedNumber(scoring, scoringPath, "perHit", rules.perHit, -kInfinity);
            rules.perKill = boundedNumber(scoring, scoringPath, "perKill", rules.perKill, -kInfinity);
            rules.perMiss = boundedNumber(scoring, scoringPath, "perMiss", rules.perMiss, -kInfinity);
            rules.perDamage = boundedNumber(scoring, scoringPath, "perDamage", rules.perDamage, -kInfinity);
            rules.hitsPerKill = intField(scoring, scoringPath, "hitsPerKill", rules.hitsPerKill, 1);
            rules.damagePerHit = boundedNumber(scoring, scoringPath, "damagePerHit", rules.damagePerHit, 0.0);

            const QString idealPath = QStringLiteral("ideal");
            if (!object.contains(idealPath)) reject(idealPath, QStringLiteral("is required"));
            const QJsonObject ideal = optionalObject(object, {}, idealPath);
            requireKnownKeys(ideal, idealPath, {"shotsPerSecond", "accuracy"});
            model.idealShotsPerSecond = boundedNumber(ideal, idealPath, "shotsPerSecond", std::nullopt, 0.0, kInfinity, true);
            model.idealAccuracy = boundedNumber(ideal, idealPath, "accuracy", std::nullopt, 0.0, 1.0, true);

            const QString skillPath = QStringLiteral("skill");
            const QJsonObject skill = optionalObject(object, {}, skillPath);
            requireKnownKeys(skill, skillPath, {"accuracy", "shotRate", "maxShotsPerSecond"});
            model.accuracyWeight = boundedNumber(skill, skillPath, "accuracy", model.accuracyWeight, 0.0);
            model.shotRateWeight = boundedNumber(skill, skillPath, "shotRate", model.shotRateWeight, 0.0);
            if (model.accuracyWeight + model.shotRateWeight <= 0.0) {
                reject(skillPath, QStringLiteral("accuracy and shotRate cannot both be 0"));
            }
            if (skill.contains("maxShotsPerSecond")) {
                model.maxShotsPerSecond = boundedNumber(skill, skillPath, "maxShotsPerSecond", std::nullopt, 0.0,
                                                        kInfinity, true);
            }
            return model;
        }

        gallery::PerformanceShape parseShapeObject(const QJsonObject &object) {
            requireKnownKeys(object, {}, {"description", "windows", "noise"});
            gallery::PerformanceShape shape;
            if (object.contains("description")) stringField(object, {}, "description", {});
            const QJsonArray windows = arrayField(object, {}, "windows");
            for (qsizetype i = 0; i < windows.size(); ++i) {
                const QString path = QStringLiteral("windows[%1]").arg(i);
                const QJsonObject window = objectAt(windows[i], path);
                requireKnownKeys(window, path, {"from", "to", "accuracy", "shotRate", "ramp"});
                gallery::PerformanceWindow parsed;
                parsed.from = boundedNumber(window, path, "from", std::nullopt, 0.0);
                parsed.to = boundedNumber(window, path, "to", std::nullopt, parsed.from,
                                          std::numeric_limits<double>::infinity(), true);
                parsed.accuracy = boundedNumber(window, path, "accuracy", parsed.accuracy, 0.0);
                parsed.shotRate = boundedNumber(window, path, "shotRate", parsed.shotRate, 0.0);
                parsed.ramp = boolField(window, path, "ramp", parsed.ramp);
                shape.windows.push_back(parsed);
            }
            const QString noisePath = QStringLiteral("noise");
            const QJsonObject noise = optionalObject(object, {}, noisePath);
            requireKnownKeys(noise, noisePath, {"accuracy", "shotRate"});
            shape.accuracyNoise = boundedNumber(noise, noisePath, "accuracy", shape.accuracyNoise, 0.0);
            shape.shotRateNoise = boundedNumber(noise, noisePath, "shotRate", shape.shotRateNoise, 0.0);
            return shape;
        }

        // Reads the JSON object a field refers to, by a path relative to the referring spec.
        QJsonObject loadReferencedObject(const QJsonObject &object, const QString &parent, const QString &key,
                                         const QString &specPath) {
            const QString path = fieldPath(parent, key);
            const QString file = QFileInfo(specPath).dir().absoluteFilePath(stringField(object, parent, key, {}, true));
            QFile input(file);
            if (!QFileInfo(file).isFile() || !input.open(QIODevice::ReadOnly)) {
                reject(path, QStringLiteral("no such file %1").arg(file));
            }
            QJsonParseError error{};
            const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) {
                reject(path, QStringLiteral("%1 is not a JSON object (%2 at offset %3)")
                                 .arg(file, error.errorString()).arg(error.offset));
            }
            return document.object();
        }

        // Errors inside the referenced file are reported under the referring field's path.
        template<typename Parse>
        auto parseReferenced(const QJsonObject &object, const QString &parent, const QString &key,
                             const QString &specPath, Parse parse) {
            const QJsonObject referenced = loadReferencedObject(object, parent, key, specPath);
            try {
                return parse(referenced);
            } catch (const SpecError &error) {
                reject(fieldPath(parent, key), error.message);
            }
        }

        Progression parseProgression(const QJsonObject &object, const QString &path) {
            requireKnownKeys(object, path, {"by", "keyframes", "fluctuation", "dayFluctuation"});
            Progression progression;
            const QString by = stringField(object, path, "by", QStringLiteral("run"));
            if (by != u"run" && by != u"day") reject(fieldPath(path, "by"), QStringLiteral("expected \"run\" or \"day\""));
            progression.byDay = by == u"day";
            const QJsonArray keyframes = arrayField(object, path, "keyframes");
            if (keyframes.isEmpty()) reject(fieldPath(path, "keyframes"), QStringLiteral("expected at least one keyframe"));
            for (qsizetype i = 0; i < keyframes.size(); ++i) {
                const QString keyframePath = QStringLiteral("%1.keyframes[%2]").arg(path).arg(i);
                const QJsonObject keyframe = objectAt(keyframes[i], keyframePath);
                requireKnownKeys(keyframe, keyframePath, {"at", "skill"});
                if (!keyframe.contains("at")) reject(fieldPath(keyframePath, "at"), QStringLiteral("is required"));
                const int at = intField(keyframe, keyframePath, "at", 0,
                                        progression.byDay ? std::numeric_limits<int>::min() : 0);
                if (i > 0 && at <= progression.keyframes.back().at) {
                    reject(fieldPath(keyframePath, "at"), QStringLiteral("expected a value after the previous keyframe's"));
                }
                progression.keyframes.push_back({static_cast<double>(at),
                                                 boundedNumber(keyframe, keyframePath, "skill", std::nullopt, 0.0)});
            }
            progression.fluctuation = boundedNumber(object, path, "fluctuation", 0.0, 0.0);
            progression.dayFluctuation = boundedNumber(object, path, "dayFluctuation", 0.0, 0.0);
            return progression;
        }

        void parseModelledFields(const QJsonObject &object, const QString &path, const QString &specPath,
                                 ScenarioSpec &scenario) {
            if (object.contains("shape")) {
                reject(fieldPath(path, "shape"), QStringLiteral("not with scenarioSpec (use performanceSpec)"));
            }
            ModelledScenario modelled;
            modelled.model = parseReferenced(object, path, "scenarioSpec", specPath, parseModelObject);
            // Renaming a shared scenario-spec without a hash makes a scenario of its own.
            if (object.contains("name")) {
                const QString name = stringField(object, path, "name", {});
                modelled.model.name = name.toStdString();
                modelled.model.hash = "spec-" + slug(name);
            }
            modelled.model.hash = stringField(object, path, "hash", QString::fromStdString(modelled.model.hash)).toStdString();
            modelled.model.durationSeconds = boundedNumber(object, path, "durationSeconds",
                                                           modelled.model.durationSeconds, 1.0);
            scenario.name = modelled.model.name;
            scenario.hash = modelled.model.hash;
            scenario.durationSeconds = modelled.model.durationSeconds;

            if (object.contains("performanceSpec")) {
                modelled.shape = parseReferenced(object, path, "performanceSpec", specPath, parseShapeObject);
            }
            if (object.contains("skill") && object.contains("progression")) {
                reject(path, QStringLiteral("give either skill or progression, not both"));
            }
            modelled.skill = boundedNumber(object, path, "skill", modelled.skill, 0.0);
            if (object.contains("progression")) {
                const QString progressionPath = fieldPath(path, "progression");
                modelled.progression = parseProgression(objectAt(object.value("progression"), progressionPath),
                                                        progressionPath);
            }
            if (object.contains("idealScore")) {
                const double target = boundedNumber(object, path, "idealScore", std::nullopt, 0.0,
                                                    std::numeric_limits<double>::infinity(), true);
                const double natural = gallery::idealScore(modelled.model);
                if (natural <= 0.0) {
                    reject(fieldPath(path, "idealScore"), QStringLiteral("the scenario-spec's ideal run scores %1, "
                                                                         "so it cannot be rescaled").arg(natural));
                }
                modelled.scoreScale = target / natural;
            }

            const QJsonArray handRuns = arrayField(object, path, "handRuns");
            for (qsizetype i = 0; i < handRuns.size(); ++i) {
                const QString handPath = QStringLiteral("%1.handRuns[%2]").arg(path).arg(i);
                const QJsonObject hand = objectAt(handRuns[i], handPath);
                requireKnownKeys(hand, handPath, {"run", "skill", "performanceSpec"});
                if (!hand.contains("run")) reject(fieldPath(handPath, "run"), QStringLiteral("is required"));
                HandRun parsed;
                parsed.run = intField(hand, handPath, "run", 0, 0, std::max(0, scenario.runs - 1));
                if (parsed.run >= scenario.runs) {
                    reject(fieldPath(handPath, "run"), QStringLiteral("the scenario has %1 runs").arg(scenario.runs));
                }
                if (std::ranges::any_of(modelled.handRuns, [&](const HandRun &other) { return other.run == parsed.run; })) {
                    reject(fieldPath(handPath, "run"), QStringLiteral("run %1 is already hand-specified").arg(parsed.run));
                }
                if (hand.contains("skill")) parsed.skill = boundedNumber(hand, handPath, "skill", std::nullopt, 0.0);
                if (hand.contains("performanceSpec")) {
                    parsed.shape = parseReferenced(hand, handPath, "performanceSpec", specPath, parseShapeObject);
                }
                modelled.handRuns.push_back(std::move(parsed));
            }
            scenario.modelled = std::move(modelled);
        }

        ScenarioSpec parseScenario(const QJsonObject &object, const QString &path, const QString &specPath) {
            requireKnownKeys(object, path, {"name", "hash", "runs", "firstDay", "daySpacing", "runsPerDay",
                                            "startMinute", "minuteSpacing", "durationSeconds", "performance", "shape",
                                            "scenarioSpec", "performanceSpec", "idealScore", "skill", "progression",
                                            "handRuns"});
            ScenarioSpec scenario;
            scenario.runs = intField(object, path, "runs", scenario.runs, 0);
            scenario.firstDay = intField(object, path, "firstDay", scenario.firstDay, std::numeric_limits<int>::min());
            scenario.daySpacing = intField(object, path, "daySpacing", scenario.daySpacing, 0);
            scenario.runsPerDay = intField(object, path, "runsPerDay", scenario.runsPerDay, 1);
            scenario.startMinute = intField(object, path, "startMinute", scenario.startMinute, 0, 24 * 60 - 1);
            scenario.minuteSpacing = intField(object, path, "minuteSpacing", scenario.minuteSpacing, 0);
            scenario.performance = boolField(object, path, "performance", scenario.performance);
            if (object.contains("scenarioSpec")) {
                parseModelledFields(object, path, specPath, scenario);
                return scenario;
            }
            for (const char *key: {"performanceSpec", "idealScore", "skill", "progression", "handRuns"}) {
                if (object.contains(key)) reject(fieldPath(path, key), QStringLiteral("needs scenarioSpec"));
            }
            const QString name = stringField(object, path, "name", {}, true);
            scenario.name = name.toStdString();
            scenario.hash = stringField(object, path, "hash", QString::fromStdString("spec-" + slug(name))).toStdString();
            scenario.durationSeconds = numberField(object, path, "durationSeconds", scenario.durationSeconds, 1.0);
            if (object.contains("shape")) scenario.shape = parseShape(object.value("shape"), fieldPath(path, "shape"));
            return scenario;
        }

        const std::vector<BenchmarkTierSpec> &defaultTiers() {
            static const std::vector<BenchmarkTierSpec> kTiers{
                {"iron", "Iron", {130, 130, 140, 255}, 0.6},
                {"bronze", "Bronze", {205, 127, 50, 255}, 0.8},
                {"silver", "Silver", {192, 192, 200, 255}, 0.95},
                {"gold", "Gold", {255, 200, 40, 255}, 1.05},
                {"platinum", "Platinum", {80, 220, 220, 255}, 1.2},
            };
            return kTiers;
        }

        BenchmarkSpec parseBenchmark(const QJsonObject &object, const std::vector<ScenarioSpec> &scenarios) {
            const QString path = QStringLiteral("benchmark");
            requireKnownKeys(object, path, {"id", "name", "file", "tiers", "tierFactors", "categories"});
            BenchmarkSpec spec;
            spec.id = stringField(object, path, "id", QString::fromStdString(spec.id)).toStdString();
            spec.name = stringField(object, path, "name", QString::fromStdString(spec.name)).toStdString();
            spec.file = stringField(object, path, "file", QString::fromStdString(spec.id + ".json")).toStdString();
            if (object.contains("tiers") && object.contains("tierFactors")) {
                reject(path, QStringLiteral("give either tiers or tierFactors, not both"));
            }
            const QJsonArray tiers = arrayField(object, path, "tiers");
            for (qsizetype i = 0; i < tiers.size(); ++i) {
                const QString tierPath = QStringLiteral("%1.tiers[%2]").arg(path).arg(i);
                const QJsonObject tier = objectAt(tiers[i], tierPath);
                requireKnownKeys(tier, tierPath, {"id", "name", "factor", "color"});
                const QString name = stringField(tier, tierPath, "name", {}, true);
                const auto &fallback = defaultTiers()[std::min<std::size_t>(i, defaultTiers().size() - 1)];
                spec.tiers.push_back({stringField(tier, tierPath, "id", QString::fromStdString(slug(name))).toStdString(),
                                      name.toStdString(), colorField(tier, tierPath, "color", fallback.color),
                                      numberField(tier, tierPath, "factor", 1.0, 0.0)});
            }
            const QJsonArray factors = arrayField(object, path, "tierFactors");
            if (factors.size() > static_cast<qsizetype>(defaultTiers().size())) {
                reject(path + QStringLiteral(".tierFactors"), QStringLiteral("at most %1 factors").arg(defaultTiers().size()));
            }
            for (qsizetype i = 0; i < factors.size(); ++i) {
                if (!factors[i].isDouble() || factors[i].toDouble() < 0) {
                    reject(QStringLiteral("%1.tierFactors[%2]").arg(path).arg(i), QStringLiteral("expected a number >= 0"));
                }
                BenchmarkTierSpec tier = defaultTiers()[i];
                tier.factor = factors[i].toDouble();
                spec.tiers.push_back(tier);
            }
            if (!object.contains("tiers") && !object.contains("tierFactors")) spec.tiers = defaultTiers();

            const QJsonArray categories = arrayField(object, path, "categories");
            for (qsizetype i = 0; i < categories.size(); ++i) {
                const QString categoryPath = QStringLiteral("%1.categories[%2]").arg(path).arg(i);
                const QJsonObject category = objectAt(categories[i], categoryPath);
                requireKnownKeys(category, categoryPath, {"id", "name", "color", "scenarios"});
                BenchmarkCategorySpec parsed;
                const QString name = stringField(category, categoryPath, "name", {}, true);
                parsed.name = name.toStdString();
                parsed.id = stringField(category, categoryPath, "id", QString::fromStdString(slug(name))).toStdString();
                parsed.color = colorField(category, categoryPath, "color", {0, 188, 212, 255});
                const QJsonArray refs = arrayField(category, categoryPath, "scenarios");
                for (qsizetype k = 0; k < refs.size(); ++k) {
                    const QString refPath = QStringLiteral("%1.scenarios[%2]").arg(categoryPath).arg(k);
                    if (refs[k].isString()) {
                        parsed.scenarios.push_back({refs[k].toString().toStdString(), std::nullopt});
                        continue;
                    }
                    const QJsonObject ref = objectAt(refs[k], refPath);
                    requireKnownKeys(ref, refPath, {"name", "hash"});
                    std::optional<std::string> hash;
                    if (ref.contains("hash")) hash = stringField(ref, refPath, "hash", {}).toStdString();
                    parsed.scenarios.push_back({stringField(ref, refPath, "name", {}, true).toStdString(), hash});
                }
                spec.categories.push_back(parsed);
            }
            if (!object.contains("categories")) {
                BenchmarkCategorySpec all{"all", "All", {0, 188, 212, 255}, {}};
                for (const auto &scenario: scenarios) all.scenarios.push_back({scenario.name, scenario.hash});
                spec.categories.push_back(all);
            }
            return spec;
        }

        double trendAt(const Progression &progression, const double at) {
            const auto &keyframes = progression.keyframes;
            if (at <= keyframes.front().at) return keyframes.front().skill;
            if (at >= keyframes.back().at) return keyframes.back().skill;
            const auto next = std::ranges::find_if(keyframes, [&](const SkillKeyframe &keyframe) { return keyframe.at > at; });
            const auto previous = std::prev(next);
            return previous->skill + (next->skill - previous->skill) * (at - previous->at) / (next->at - previous->at);
        }

        // Distinct streams per purpose, so adding fluctuation never changes a run's shape noise.
        enum class Stream : std::uint64_t { Run = 1, DayForm = 2, RunForm = 3 };

        domain::Run makeModelledSpecRun(const ModelledScenario &modelled, const domain::ScenarioId &id,
                                        const std::size_t scenarioIndex, const int runIndex, const int day,
                                        const int minute) {
            const auto stream = [&](const Stream kind, const long long value) {
                return gallery::mixSeed({scenarioIndex, static_cast<std::uint64_t>(kind), static_cast<std::uint64_t>(value)});
            };
            const auto hand = std::ranges::find_if(modelled.handRuns, [&](const HandRun &candidate) {
                return candidate.run == runIndex;
            });
            const bool handSpecified = hand != modelled.handRuns.end();
            const auto &progression = modelled.progression;

            double skill = progression ? trendAt(*progression, progression->byDay ? day : runIndex) : modelled.skill;
            if (handSpecified) {
                skill = hand->skill.value_or(skill);
            } else if (progression) {
                skill *= std::max(0.0, 1.0 + progression->dayFluctuation *
                                                 gallery::SpecRandom(stream(Stream::DayForm, day)).normal());
                skill *= std::max(0.0, 1.0 + progression->fluctuation *
                                                 gallery::SpecRandom(stream(Stream::RunForm, runIndex)).normal());
            }
            const gallery::PerformanceShape &shape = handSpecified && hand->shape ? *hand->shape : modelled.shape;
            return gallery::makeModelledRun(id, day, minute, modelled.model, shape, skill, modelled.scoreScale,
                                            stream(Stream::Run, runIndex));
        }
    }

    domain::Benchmark generateBenchmark(const domain::UserProfile &profile, const BenchmarkSpec &spec) {
        std::map<std::string, double> best;
        std::map<std::string, std::set<std::string>> hashesByName;
        for (const auto &run: profile.getAllRunRecords()) {
            auto &score = best[run.run_id.scenario_id.hash];
            score = std::max(score, static_cast<double>(run.totals().score));
            hashesByName[run.run_id.scenario_id.name].insert(run.run_id.scenario_id.hash);
        }

        domain::Benchmark benchmark;
        benchmark.id = domain::BenchmarkId{spec.id};
        benchmark.name = spec.name;
        for (const auto &tier: spec.tiers) benchmark.tiers.push_back({domain::TierId{tier.id}, tier.name, tier.color});

        QSet<QString> usedIds;
        const auto entry = [&](const BenchmarkScenarioRef &ref) {
            domain::ScenarioEntry result;
            result.name = ref.name;
            result.hash = ref.hash;
            // A name maps to its scenario only when exactly one scenario has it; a
            // shared name stays unresolved, as the app would leave it ambiguous.
            if (!result.hash) {
                if (const auto found = hashesByName.find(ref.name); found != hashesByName.end() && found->second.size() == 1) {
                    result.hash = *found->second.begin();
                }
            }
            std::string id = "entry-" + (result.hash ? *result.hash : slug(QString::fromStdString(ref.name)));
            for (int suffix = 2; usedIds.contains(QString::fromStdString(id)); ++suffix) {
                id = "entry-" + (result.hash ? *result.hash : slug(QString::fromStdString(ref.name))) + "-" +
                     std::to_string(suffix);
            }
            usedIds.insert(QString::fromStdString(id));
            result.id = domain::ScenarioEntryId{id};
            const auto found = result.hash ? best.find(*result.hash) : best.end();
            const double base = found != best.end() && found->second > 0 ? found->second : 1000.0;
            for (const auto &tier: spec.tiers) {
                result.thresholds.push_back({domain::TierId{tier.id}, std::round(base * tier.factor)});
            }
            return result;
        };
        for (const auto &category: spec.categories) {
            domain::Category generated{domain::GroupId{category.id}, category.name, category.color, {}, {}};
            for (const auto &ref: category.scenarios) generated.scenarios.push_back(entry(ref));
            benchmark.categories.push_back(generated);
        }
        return benchmark;
    }

    QString parseProfileSpec(const QJsonObject &json, const QString &specPath, ProfileSpec &out) {
        try {
            requireKnownKeys(json, {}, {"scenarios", "benchmark"});
            ProfileSpec spec;
            const QJsonArray scenarios = arrayField(json, {}, "scenarios");
            for (qsizetype i = 0; i < scenarios.size(); ++i) {
                const QString path = QStringLiteral("scenarios[%1]").arg(i);
                spec.scenarios.push_back(parseScenario(objectAt(scenarios[i], path), path, specPath));
            }
            if (json.contains("benchmark")) {
                const QJsonValue benchmark = json.value("benchmark");
                if (benchmark.isString()) {
                    const QString file = QFileInfo(specPath).dir().absoluteFilePath(benchmark.toString());
                    if (!QFileInfo(file).isFile()) reject("benchmark", QStringLiteral("no such file %1").arg(file));
                    spec.benchmarkFile = file;
                } else {
                    spec.benchmark = parseBenchmark(objectAt(benchmark, "benchmark"), spec.scenarios);
                }
            }
            out = std::move(spec);
            return {};
        } catch (const SpecError &error) {
            return QStringLiteral("profile spec %1: %2").arg(specPath, error.message);
        }
    }

    QString parseScenarioModel(const QJsonObject &json, gallery::ScenarioModel &out) {
        try {
            out = parseModelObject(json);
            return {};
        } catch (const SpecError &error) {
            return error.message;
        }
    }

    QString parsePerformanceShape(const QJsonObject &json, gallery::PerformanceShape &out) {
        try {
            out = parseShapeObject(json);
            return {};
        } catch (const SpecError &error) {
            return error.message;
        }
    }

    domain::UserProfile buildProfile(const ProfileSpec &spec) {
        domain::UserProfile profile;
        for (std::size_t scenarioIndex = 0; scenarioIndex < spec.scenarios.size(); ++scenarioIndex) {
            const ScenarioSpec &scenario = spec.scenarios[scenarioIndex];
            const domain::ScenarioId id{.name = scenario.name, .hash = scenario.hash};
            for (int runIndex = 0; runIndex < scenario.runs; ++runIndex) {
                const int day = scenario.firstDay + (runIndex / scenario.runsPerDay) * scenario.daySpacing;
                const int minute = scenario.startMinute + (runIndex % scenario.runsPerDay) * scenario.minuteSpacing;
                domain::Run run;
                if (scenario.modelled) {
                    run = makeModelledSpecRun(*scenario.modelled, id, scenarioIndex, runIndex, day, minute);
                } else {
                    const int seed = static_cast<int>(scenarioIndex) * 1000 + runIndex;
                    run = gallery::makeRun(id, day, minute, static_cast<float>(scenario.durationSeconds), seed,
                                           scenario.shape);
                }
                // A run without a performance block keeps only its totals, as a
                // stats-CSV-only import does.
                if (!scenario.performance) run.performance.reset();
                profile.addRun(run);
            }
        }
        return profile;
    }

    QString writeProfileSpec(const QString &specPath, const QString &profilePath, const QString &benchmarksDir) {
        QFile file(specPath);
        if (!file.open(QIODevice::ReadOnly)) return QStringLiteral("could not read profile spec %1").arg(specPath);
        QJsonParseError error{};
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            return QStringLiteral("profile spec %1 is not a JSON object (%2 at offset %3)")
                .arg(specPath, error.errorString()).arg(error.offset);
        }
        ProfileSpec spec;
        if (const QString parseError = parseProfileSpec(document.object(), specPath, spec); !parseError.isEmpty()) {
            return parseError;
        }
        const domain::UserProfile profile = buildProfile(spec);
        // ProfileSerializer stamps a new store with the current time but keeps the
        // created_at of a store it overwrites. Seed a header-only file with the
        // spec epoch so the same spec always writes the same bytes.
        {
            profile::File seed;
            seed.mutable_header()->set_created_at(kSpecEpochSeconds);
            std::ofstream output(profilePath.toStdString(), std::ios::binary | std::ios::trunc);
            if (!output || !seed.SerializeToOstream(&output)) return QStringLiteral("could not write %1").arg(profilePath);
        }
        if (!data::ProfileSerializer().save(profile, profilePath.toStdString())) {
            return QStringLiteral("could not write %1").arg(profilePath);
        }
        if (!QDir().mkpath(benchmarksDir)) return QStringLiteral("could not create %1").arg(benchmarksDir);
        if (!spec.benchmarkFile.isEmpty()) {
            const QString target = QDir(benchmarksDir).filePath(QFileInfo(spec.benchmarkFile).fileName());
            QFile::remove(target);
            if (!QFile::copy(spec.benchmarkFile, target)) return QStringLiteral("could not copy %1").arg(spec.benchmarkFile);
            QFile::setPermissions(target, QFile::permissions(target) | QFileDevice::WriteOwner | QFileDevice::WriteUser);
        } else if (spec.benchmark) {
            qt_data::BenchmarkStore store(benchmarksDir.toStdString());
            if (!store.write(generateBenchmark(profile, *spec.benchmark), spec.benchmark->file, std::nullopt).succeeded()) {
                return QStringLiteral("could not write the spec's benchmark %1").arg(QString::fromStdString(spec.benchmark->file));
            }
        }
        return {};
    }
}
