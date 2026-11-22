#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <variant>

#include "qt_data/benchmark_store.h"

using namespace ksv;
using namespace ksv::data;
using namespace ksv::qt_data;

namespace {
    // A minimal, valid, Trackable benchmark document at schemaVersion 3.
    QByteArray trackableJson() {
        return QByteArray(R"({
            "schemaVersion": 3,
            "tiers": [ { "id": "t1", "name": "Bronze", "color": [200, 120, 0, 255] } ],
            "uncategorized": [
                { "id": "s1", "name": "Scenario One", "hashes": [],
                  "thresholds": [ { "tierId": "t1", "score": 100.0 } ] }
            ],
            "categories": []
        })");
    }

    QByteArray categorizedJson() {
        return QByteArray(R"({
            "schemaVersion": 3,
            "tiers": [ { "id": "t1", "name": "Bronze", "color": [200, 120, 0, 255] } ],
            "uncategorized": [],
            "categories": [
                { "id": "c1", "name": "Category", "color": [10, 20, 30, 255],
                  "scenarios": [
                      { "id": "s1", "name": "One", "hashes": [],
                        "thresholds": [ { "tierId": "t1", "score": 1.0 } ] }
                  ],
                  "subcategories": [
                      { "id": "sc1", "name": "Sub", "color": [40, 50, 60, 255],
                        "scenarios": [
                            { "id": "s2", "name": "Two", "hashes": [],
                              "thresholds": [ { "tierId": "t1", "score": 2.0 } ] }
                        ] }
                  ] }
            ]
        })");
    }

    QByteArray withTopLevel(const QByteArray &json, const char *key, const QJsonValue &value) {
        auto object = QJsonDocument::fromJson(json).object();
        object.insert(QLatin1String(key), value);
        return QJsonDocument(object).toJson();
    }

    QByteArray withoutTopLevel(const QByteArray &json, const char *key) {
        auto object = QJsonDocument::fromJson(json).object();
        object.remove(QLatin1String(key));
        return QJsonDocument(object).toJson();
    }

    domain::BenchmarkDefinition writableDefinition(const std::string &tierName = "Bronze") {
        domain::BenchmarkDefinition def;
        def.tiers = {domain::Tier{domain::TierId{"t1"}, tierName, {}}};
        def.uncategorized = {domain::BenchmarkScenario{domain::BenchmarkScenarioId{"s1"}, "One", {},
                                                   {domain::Threshold{domain::TierId{"t1"}, 10.0}}}};
        return def;
    }

    class BenchmarkStoreTest : public testing::Test {
    protected:
        QTemporaryDir dir;
        std::unique_ptr<BenchmarkStore> store;

        void SetUp() override {
            ASSERT_TRUE(dir.isValid());
            store = std::make_unique<BenchmarkStore>(dir.path().toStdString());
        }

        QString write(const QString &name, const QByteArray &bytes) const {
            const auto path = QDir(dir.path()).absoluteFilePath(name);
            QFile file(path);
            EXPECT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(bytes);
            return path;
        }

        [[nodiscard]] std::optional<QByteArray> read(const QString &name) const {
            QFile file(QDir(dir.path()).absoluteFilePath(name));
            if (!file.exists() || !file.open(QIODevice::ReadOnly)) return std::nullopt;
            return file.readAll();
        }

        [[nodiscard]] QStringList listing() const {
            return QDir(dir.path()).entryList({"*.json"}, QDir::Files, QDir::Name);
        }

        [[nodiscard]] BenchmarkFileEntry onlyEntry() const {
            const auto entries = store->scan().snapshot->entries;
            EXPECT_EQ(entries.size(), 1U);
            return entries.at(0);
        }

        [[nodiscard]] BenchmarkFileEntry entryNamed(const std::string &filename) const {
            for (const auto &entry: store->scan().snapshot->entries)
                if (entry.filename == filename) return entry;
            ADD_FAILURE() << "no entry " << filename;
            return {};
        }
    };

    const LoadedBenchmark *loaded(const BenchmarkFileEntry &entry) {
        return std::get_if<LoadedBenchmark>(&entry.content);
    }
    const ProblemBenchmark *problem(const BenchmarkFileEntry &entry) {
        return std::get_if<ProblemBenchmark>(&entry.content);
    }
}

TEST_F(BenchmarkStoreTest, ScanClassifiesAValidTrackableFile) {
    write("bench.json", trackableJson());
    const auto result = store->scan();
    ASSERT_TRUE(result.snapshot.has_value());
    ASSERT_EQ(result.snapshot->entries.size(), 1U);
    const auto &entry = result.snapshot->entries[0];
    EXPECT_EQ(entry.filename, "bench.json");
    EXPECT_FALSE(entry.digest.empty());
    ASSERT_NE(loaded(entry), nullptr);
    // The store never names a benchmark; BenchmarksService names it after its file.
    std::vector<domain::BenchmarkIssueCode> codes;
    for (const auto &issue: loaded(entry)->completeness.issues) codes.push_back(issue.code);
    EXPECT_EQ(codes, std::vector{domain::BenchmarkIssueCode::MissingName});
}

TEST_F(BenchmarkStoreTest, ReadsMultipleHashesAndPreservesThemOnWrite) {
    auto json = trackableJson();
    json.replace("\"hashes\": []", "\"hashes\": [\"old\", \"new\"]");
    write("linked.json", json);
    const auto entry = onlyEntry();
    ASSERT_NE(loaded(entry), nullptr);
    const auto saved = store->write(loaded(entry)->benchmark, "linked.json", entry.digest);
    ASSERT_TRUE(saved.succeeded());
    const auto document = QJsonDocument::fromJson(*read("linked.json")).object();
    EXPECT_EQ(document.value("uncategorized").toArray().at(0).toObject().value("hashes").toArray(),
              (QJsonArray{"old", "new"}));
    const auto reread = onlyEntry();
    ASSERT_NE(loaded(reread), nullptr);
    EXPECT_EQ(loaded(reread)->benchmark, loaded(entry)->benchmark);
}

TEST_F(BenchmarkStoreTest, RejectsMalformedHashArrays) {
    for (const QByteArray hashes: {QByteArray{"null"}, QByteArray{"\"old\""}, QByteArray{"[1]"},
                                  QByteArray{"[\"\"]"}, QByteArray{"[\"old\",\"old\"]"}}) {
        auto json = trackableJson();
        json.replace("\"hashes\": []", QByteArray{"\"hashes\": "} + hashes);
        write("invalid.json", json);
        const auto entry = onlyEntry();
        ASSERT_NE(problem(entry), nullptr) << hashes.toStdString();
        EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Invalid) << hashes.toStdString();
    }
}

TEST_F(BenchmarkStoreTest, WritesSchemaThreeWithNoTopLevelIdOrName) {
    auto def = writableDefinition();
    def.id = domain::BenchmarkId{"in-memory-key"};
    def.name = "Working name";
    ASSERT_TRUE(store->write(def, "written.json", std::nullopt).succeeded());

    const auto document = QJsonDocument::fromJson(*read("written.json")).object();
    EXPECT_EQ(document.keys(), (QStringList{"categories", "schemaVersion", "tiers", "uncategorized"}));
    EXPECT_EQ(document.value("schemaVersion").toInt(), 3);
    EXPECT_NE(loaded(onlyEntry()), nullptr);
}

TEST_F(BenchmarkStoreTest, SchemaThreeWithAnyOtherTopLevelKeySetIsInvalid) {
    const std::vector<std::pair<std::string, QByteArray>> cases{
        {"extra id", withTopLevel(trackableJson(), "id", "b1")},
        {"extra name", withTopLevel(trackableJson(), "name", "Bench")},
        {"unknown key", withTopLevel(trackableJson(), "notes", "x")},
        {"missing key", withoutTopLevel(trackableJson(), "categories")},
    };
    for (const auto &[label, json]: cases) {
        write("keys.json", json);
        const auto entry = onlyEntry();
        ASSERT_NE(problem(entry), nullptr) << label;
        EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Invalid) << label;
    }
}

TEST_F(BenchmarkStoreTest, NewerSchemaVersionIsUnsupportedWithItsVersion) {
    auto json = trackableJson();
    json.replace("\"schemaVersion\": 3", "\"schemaVersion\": 4");
    write("future.json", json);
    const auto entry = onlyEntry();
    ASSERT_NE(problem(entry), nullptr);
    EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Unsupported);
    EXPECT_EQ(problem(entry)->schemaVersion, std::make_optional(4));
}

TEST_F(BenchmarkStoreTest, ScanReportsMissingDirectoryFailureOnceCreationIsImpossible) {
    // Point at a path whose parent is a file, so mkpath cannot succeed.
    write("blocker", QByteArray("x"));
    BenchmarkStore blocked(QDir(dir.path()).absoluteFilePath("blocker/child").toStdString());
    const auto result = blocked.scan();
    EXPECT_FALSE(result.snapshot.has_value());
    ASSERT_TRUE(result.failure.has_value());
    EXPECT_EQ(*result.failure, BenchmarkScanFailure::DirectoryUnavailable);
}

TEST_F(BenchmarkStoreTest, NonObjectsAndBadVersionsAreInvalid) {
    const std::vector<std::pair<std::string, QByteArray>> cases{
        {"malformed json", QByteArray("{ not json")},
        {"array", QByteArray("[1, 2]")},
        {"no version", withoutTopLevel(trackableJson(), "schemaVersion")},
        {"string version", withTopLevel(trackableJson(), "schemaVersion", "3")},
        {"fractional version", withTopLevel(trackableJson(), "schemaVersion", 2.5)},
        {"version zero", withTopLevel(trackableJson(), "schemaVersion", 0)},
    };
    for (const auto &[label, json]: cases) {
        write("bad.json", json);
        const auto entry = onlyEntry();
        ASSERT_NE(problem(entry), nullptr) << label;
        EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Invalid) << label;
    }
}

TEST_F(BenchmarkStoreTest, DanglingTierReferenceIsInvalid) {
    auto json = trackableJson();
    json.replace("\"tierId\": \"t1\"", "\"tierId\": \"tX\"");  // references a tier that isn't declared
    write("dangling.json", json);
    const auto entry = onlyEntry();
    ASSERT_NE(problem(entry), nullptr);
    EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Invalid);
}

TEST_F(BenchmarkStoreTest, StructurallyCompleteButMissingThresholdLoadsAsIncomplete) {
    auto json = trackableJson();
    json.replace("\"thresholds\": [ { \"tierId\": \"t1\", \"score\": 100.0 } ]", "\"thresholds\": []");
    write("incomplete.json", json);
    const auto entry = onlyEntry();
    ASSERT_NE(loaded(entry), nullptr);
    EXPECT_EQ(loaded(entry)->completeness.completeness, domain::Completeness::Incomplete);
}

TEST_F(BenchmarkStoreTest, OneBadFileDoesNotBlockAValidSibling) {
    write("aaa_bad.json", QByteArray("{ broken"));
    write("bbb_good.json", trackableJson());
    const auto entries = store->scan().snapshot->entries;
    ASSERT_EQ(entries.size(), 2U);
    EXPECT_NE(problem(entries.at(0)), nullptr);  // sorted by name: aaa first
    EXPECT_NE(loaded(entries.at(1)), nullptr);
}

TEST_F(BenchmarkStoreTest, DuplicateStableIdWithinAFileIsInvalid) {
    auto json = trackableJson();
    json.replace("\"id\": \"s1\"", "\"id\": \"t1\"");  // scenario reuses the tier's id
    write("dupe.json", json);
    const auto entry = onlyEntry();
    ASSERT_NE(problem(entry), nullptr);
    EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Invalid);
}

TEST_F(BenchmarkStoreTest, CategorizedFixtureLoads) {
    write("categorized.json", categorizedJson());
    EXPECT_NE(loaded(onlyEntry()), nullptr);
}

TEST_F(BenchmarkStoreTest, DuplicateEntryIdAcrossACategoryAndItsSubcategoryIsInvalid) {
    auto json = categorizedJson();
    json.replace("\"id\": \"s2\"", "\"id\": \"s1\"");
    write("dupe-entry.json", json);
    const auto entry = onlyEntry();
    ASSERT_NE(problem(entry), nullptr);
    EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Invalid);
}

TEST_F(BenchmarkStoreTest, DanglingTierReferenceInASubcategoryEntryIsInvalid) {
    auto json = categorizedJson();
    json.replace("\"tierId\": \"t1\", \"score\": 2.0", "\"tierId\": \"tX\", \"score\": 2.0");
    write("dangling-nested.json", json);
    const auto entry = onlyEntry();
    ASSERT_NE(problem(entry), nullptr);
    EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Invalid);
}

TEST_F(BenchmarkStoreTest, DuplicateGroupIdBetweenACategoryAndItsSubcategoryIsInvalid) {
    auto json = categorizedJson();
    json.replace("\"id\": \"sc1\"", "\"id\": \"c1\"");
    write("dupe-group.json", json);
    const auto entry = onlyEntry();
    ASSERT_NE(problem(entry), nullptr);
    EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Invalid);
}

TEST_F(BenchmarkStoreTest, WriteCreatesThenRoundTripsThroughScan) {
    const auto write = store->write(writableDefinition(), "written.json", std::nullopt);
    ASSERT_TRUE(write.succeeded());

    const auto entry = onlyEntry();
    ASSERT_NE(loaded(entry), nullptr);
    EXPECT_EQ(loaded(entry)->benchmark, writableDefinition());
    EXPECT_EQ(entry.digest, *write.digest);  // scan digest matches the write's returned digest
}

TEST_F(BenchmarkStoreTest, IncompleteWorkRoundTripsExactlyAndKeepsItsIssues) {
    using namespace ksv::domain;
    BenchmarkDefinition def;
    def.tiers = {Tier{TierId{"t1"}, "Bronze", {200, 120, 0, 255}},
                 Tier{TierId{"t2"}, "Silver", {190, 190, 190, 255}}};
    // Partial thresholds: t2 still missing.
    def.uncategorized = {BenchmarkScenario{BenchmarkScenarioId{"s1"}, "Loose", {},
                                       {Threshold{TierId{"t1"}, 10.0}}}};
    Category nested{GroupId{"c1"}, "Tracking", {10, 20, 30, 255}, {}, {}};
    nested.subcategories = {Subcategory{GroupId{"sc1"}, "Smooth", {40, 50, 60, 255},
                                        {BenchmarkScenario{BenchmarkScenarioId{"s2"}, "Mapped", std::vector<std::string>{"HASH", "OTHER"},
                                                       {Threshold{TierId{"t1"}, 1.5},
                                                        Threshold{TierId{"t2"}, 2.5}}}}}};
    def.categories = {nested, Category{GroupId{"c2"}, "Not yet filled", {70, 80, 90, 255}, {}, {}}};

    ASSERT_TRUE(store->write(def, "half.json", std::nullopt).succeeded());

    const auto entry = onlyEntry();
    const auto *reread = loaded(entry);
    ASSERT_NE(reread, nullptr);
    EXPECT_EQ(reread->benchmark, def);  // every entered value, colour, mapping and placement survives
    EXPECT_EQ(reread->completeness.completeness, Completeness::Incomplete);
    EXPECT_EQ(reread->completeness, validateBenchmark(def));
    const auto has = [&](BenchmarkIssueCode code) {
        return std::ranges::any_of(reread->completeness.issues,
                                   [&](const auto &issue) { return issue.code == code; });
    };
    EXPECT_TRUE(has(BenchmarkIssueCode::MissingThreshold));
    EXPECT_TRUE(has(BenchmarkIssueCode::EmptyCategory));
}

TEST_F(BenchmarkStoreTest, UnsupportedFileIsNeverRewritten) {
    auto json = trackableJson();
    json.replace("\"schemaVersion\": 3", "\"schemaVersion\": 7");
    write("future.json", json);

    for (int pass = 0; pass < 2; ++pass) {
        const auto entry = onlyEntry();
        ASSERT_NE(problem(entry), nullptr);
        EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Unsupported);
    }
    EXPECT_EQ(store->write(writableDefinition(), "future.json", std::nullopt).failure,
              std::make_optional(BenchmarkWriteFailure::ExternalModificationConflict));
    EXPECT_EQ(read("future.json"), std::make_optional(json));
}

TEST_F(BenchmarkStoreTest, ReplaceWithStaleDigestIsAConflictAndDoesNotOverwrite) {
    write("f.json", trackableJson());
    const auto original = onlyEntry();

    const auto result = store->write(writableDefinition("Edited"), "f.json", std::string{"deadbeef"});
    ASSERT_FALSE(result.succeeded());
    EXPECT_EQ(*result.failure, BenchmarkWriteFailure::ExternalModificationConflict);
    EXPECT_EQ(read("f.json"), std::make_optional(trackableJson()));
    EXPECT_EQ(onlyEntry().digest, original.digest);
}

TEST_F(BenchmarkStoreTest, ReplaceWithMatchingDigestSucceeds) {
    write("f.json", trackableJson());
    const auto original = onlyEntry();

    ASSERT_TRUE(store->write(writableDefinition("Edited"), "f.json", original.digest).succeeded());
    const auto entry = onlyEntry();
    ASSERT_NE(loaded(entry), nullptr);
    EXPECT_EQ(loaded(entry)->benchmark.tiers.at(0).name, "Edited");
}

TEST_F(BenchmarkStoreTest, RemoveRequiresMatchingDigest) {
    write("f.json", trackableJson());
    const auto digest = onlyEntry().digest;
    EXPECT_EQ(store->remove("f.json", "wrong").failure, BenchmarkWriteFailure::ExternalModificationConflict);
    EXPECT_TRUE(store->remove("f.json", digest).ok);
    EXPECT_TRUE(store->scan().snapshot->entries.empty());
}

TEST_F(BenchmarkStoreTest, RenameMovesTheFileAndKeepsItsBytes) {
    write("old.json", trackableJson());
    const auto digest = onlyEntry().digest;

    const auto result = store->rename("old.json", "new.json", digest);
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(listing(), (QStringList{"new.json"}));
    EXPECT_EQ(read("new.json"), std::make_optional(trackableJson()));
}

TEST_F(BenchmarkStoreTest, RenameWithAStaleDigestOrAMissingSourceIsAConflict) {
    write("old.json", trackableJson());

    EXPECT_EQ(store->rename("old.json", "new.json", "wrong").failure,
              std::make_optional(BenchmarkWriteFailure::ExternalModificationConflict));
    EXPECT_EQ(store->rename("absent.json", "new.json", "wrong").failure,
              std::make_optional(BenchmarkWriteFailure::ExternalModificationConflict));
    EXPECT_EQ(listing(), (QStringList{"old.json"}));
}

TEST_F(BenchmarkStoreTest, RenameOntoAnotherExistingFileIsRefusedAndTouchesNeither) {
    write("old.json", trackableJson());
    write("Taken.json", categorizedJson());
    const auto digest = entryNamed("old.json").digest;

    // The lower-case target collides on a case-insensitive file system and is a free name on a
    // case-sensitive one, so only the exact spelling is asserted on every platform.
    const auto result = store->rename("old.json", "Taken.json", digest);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.failure, std::make_optional(BenchmarkWriteFailure::WriteFailed));
#ifdef Q_OS_WIN
    EXPECT_FALSE(store->rename("old.json", "taken.json", digest).ok);
#endif
    EXPECT_EQ(read("old.json"), std::make_optional(trackableJson()));
    EXPECT_EQ(read("Taken.json"), std::make_optional(categorizedJson()));
}

TEST_F(BenchmarkStoreTest, RenameChangingOnlyLetterCaseRespellsTheFile) {
    write("bench.json", trackableJson());
    const auto digest = onlyEntry().digest;

    EXPECT_TRUE(store->rename("bench.json", "Bench.json", digest).ok);
    EXPECT_EQ(listing(), (QStringList{"Bench.json"}));
    EXPECT_EQ(onlyEntry().filename, "Bench.json");
}
