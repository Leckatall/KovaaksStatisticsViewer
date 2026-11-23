#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "qt_data/benchmark_migration/benchmark_migration.h"

using namespace ksv::qt_data::benchmark_migration;

namespace {
    QJsonObject parse(const char *json) { return QJsonDocument::fromJson(QByteArray(json)).object(); }

    // Every element kind, in a non-alphabetical order, so a step that drops or reorders one shows.
    QJsonObject fullV1() {
        return parse(R"({
            "schemaVersion": 1, "id": "b1", "name": "Voltaic S5",
            "tiers": [ { "id": "t2", "name": "Silver", "color": [190, 190, 190, 255] },
                       { "id": "t1", "name": "Bronze", "color": [200, 120, 0, 255] } ],
            "uncategorized": [
                { "id": "s9", "name": "Loose", "hash": "abc",
                  "thresholds": [ { "tierId": "t2", "score": 10.5 }, { "tierId": "t1", "score": 20 } ] },
                { "id": "s8", "name": "Unlinked", "hash": null, "thresholds": [] }
            ],
            "categories": [
                { "id": "c2", "name": "Clicking", "color": [10, 20, 30, 255],
                  "scenarios": [ { "id": "s7", "name": "Empty hash", "hash": "", "thresholds": [] } ],
                  "subcategories": [] },
                { "id": "c1", "name": "Tracking", "color": [40, 50, 60, 128], "scenarios": [],
                  "subcategories": [
                      { "id": "sc1", "name": "Smooth", "color": [70, 80, 90, 255],
                        "scenarios": [ { "id": "s6", "name": "Mapped", "hash": "def",
                                         "thresholds": [ { "tierId": "t1", "score": 1 } ] } ] } ] }
            ]
        })");
    }

    QJsonObject fullV2() {
        return parse(R"({
            "schemaVersion": 2, "id": "b1", "name": "Voltaic S5",
            "tiers": [ { "id": "t2", "name": "Silver", "color": [190, 190, 190, 255] },
                       { "id": "t1", "name": "Bronze", "color": [200, 120, 0, 255] } ],
            "uncategorized": [
                { "id": "s9", "name": "Loose", "hashes": ["abc"],
                  "thresholds": [ { "tierId": "t2", "score": 10.5 }, { "tierId": "t1", "score": 20 } ] },
                { "id": "s8", "name": "Unlinked", "hashes": [], "thresholds": [] }
            ],
            "categories": [
                { "id": "c2", "name": "Clicking", "color": [10, 20, 30, 255],
                  "scenarios": [ { "id": "s7", "name": "Empty hash", "hashes": [], "thresholds": [] } ],
                  "subcategories": [] },
                { "id": "c1", "name": "Tracking", "color": [40, 50, 60, 128], "scenarios": [],
                  "subcategories": [
                      { "id": "sc1", "name": "Smooth", "color": [70, 80, 90, 255],
                        "scenarios": [ { "id": "s6", "name": "Mapped", "hashes": ["def"],
                                         "thresholds": [ { "tierId": "t1", "score": 1 } ] } ] } ] }
            ]
        })");
    }

    QJsonObject fullV3() {
        auto v3 = fullV2();
        v3.remove("id");
        v3.remove("name");
        v3["schemaVersion"] = 3;
        return v3;
    }

    QJsonObject firstUncategorized(QJsonObject root, const std::function<void(QJsonObject &)> &change) {
        auto entries = root["uncategorized"].toArray();
        auto entry = entries[0].toObject();
        change(entry);
        entries[0] = entry;
        root["uncategorized"] = entries;
        return root;
    }

    QJsonObject firstTierColor(QJsonObject root, const QJsonValue &color) {
        auto tiers = root["tiers"].toArray();
        auto tier = tiers[0].toObject();
        tier["color"] = color;
        tiers[0] = tier;
        root["tiers"] = tiers;
        return root;
    }

    // Malformations of the top-level identity both older formats share.
    std::vector<std::pair<std::string, QJsonObject>> identityMismatches(const QJsonObject &valid) {
        const auto with = [&](const char *key, const QJsonValue &value) {
            auto copy = valid;
            copy[key] = value;
            return copy;
        };
        const auto without = [&](const char *key) {
            auto copy = valid;
            copy.remove(key);
            return copy;
        };
        return {{"missing name", without("name")},
                {"non-string name", with("name", 7)},
                {"missing id", without("id")},
                {"non-string id", with("id", 7)},
                {"empty id", with("id", "")},
                {"extra key", with("notes", "x")}};
    }

    std::optional<int> failedStep(const std::variant<MigratedBenchmark, BenchmarkMigrationMismatch> &result) {
        if (const auto *mismatch = std::get_if<BenchmarkMigrationMismatch>(&result)) return mismatch->fromVersion;
        return std::nullopt;
    }
}

TEST(BenchmarkV1ToV2, UpgradesEveryElementInPlaceAndTurnsSingleHashesIntoLists) {
    EXPECT_EQ(migrateV1ToV2(fullV1()), std::make_optional(fullV2()));
}

TEST(BenchmarkV2ToV3, RemovesIdAndNameAndReturnsTheStoredName) {
    const auto result = migrateV2ToV3(fullV2());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->schema3, fullV3());
    EXPECT_EQ(result->storedName, "Voltaic S5");
}

TEST(BenchmarkV2ToV3, ReturnsAnEmptyStoredNameAsEmpty) {
    auto v2 = fullV2();
    v2["name"] = "";
    const auto result = migrateV2ToV3(v2);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->storedName, "");
}

TEST(BenchmarkMigration, RunsSchemaOneThroughBothStepsAndSchemaTwoThroughOne) {
    for (const auto &[label, stored, version]: {std::tuple{"v1", fullV1(), 1}, std::tuple{"v2", fullV2(), 2}}) {
        const auto result = migrateBenchmark(stored, version);
        const auto *migrated = std::get_if<MigratedBenchmark>(&result);
        ASSERT_NE(migrated, nullptr) << label;
        EXPECT_EQ(migrated->schema3, fullV3()) << label;
        EXPECT_EQ(migrated->storedName, "Voltaic S5") << label;
    }
}

TEST(BenchmarkMigration, MalformedIdentityFailsAtTheStepThatReadsIt) {
    for (const auto &[label, stored]: identityMismatches(fullV1())) {
        EXPECT_EQ(migrateV1ToV2(stored), std::nullopt) << label;
        EXPECT_EQ(failedStep(migrateBenchmark(stored, 1)), std::make_optional(1)) << label;
    }
    for (const auto &[label, stored]: identityMismatches(fullV2())) {
        EXPECT_EQ(migrateV2ToV3(stored), std::nullopt) << label;
        EXPECT_EQ(failedStep(migrateBenchmark(stored, 2)), std::make_optional(2)) << label;
    }
}

TEST(BenchmarkMigration, AnEntryInTheOtherVersionsShapeIsAMismatch) {
    const auto v1WithHashes = firstUncategorized(fullV1(), [](QJsonObject &entry) {
        entry.remove("hash");
        entry["hashes"] = QJsonArray{"abc"};
    });
    EXPECT_EQ(failedStep(migrateBenchmark(v1WithHashes, 1)), std::make_optional(1));

    const auto v2WithHash = firstUncategorized(fullV2(), [](QJsonObject &entry) {
        entry.remove("hashes");
        entry["hash"] = "abc";
    });
    EXPECT_EQ(failedStep(migrateBenchmark(v2WithHash, 2)), std::make_optional(2));
}

TEST(BenchmarkMigration, DuplicateHashesInSchemaTwoAreAMismatch) {
    const auto duplicated = firstUncategorized(fullV2(), [](QJsonObject &entry) {
        entry["hashes"] = QJsonArray{"abc", "abc"};
    });
    EXPECT_EQ(failedStep(migrateBenchmark(duplicated, 2)), std::make_optional(2));
}

TEST(BenchmarkMigration, AMalformedColourIsAMismatch) {
    for (const QJsonValue &color: {QJsonValue{QJsonArray{1, 2, 3}}, QJsonValue{QJsonArray{1, 2, 3, 256}},
                                   QJsonValue{QJsonArray{1, 2, 3, 0.5}}, QJsonValue{"red"}}) {
        EXPECT_EQ(failedStep(migrateBenchmark(firstTierColor(fullV1(), color), 1)), std::make_optional(1));
        EXPECT_EQ(failedStep(migrateBenchmark(firstTierColor(fullV2(), color), 2)), std::make_optional(2));
    }
}
