#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <variant>

#include "qt_data/benchmark_repository.h"

using namespace ksv;
using namespace ksv::application;
using namespace ksv::qt_data;

namespace {
    // A minimal, valid, Trackable benchmark document at schemaVersion 1.
    QByteArray trackableJson(const char *id = "b1", const char *name = "Bench") {
        return QByteArray(R"({
            "schemaVersion": 1,
            "id": ")") + id + R"(",
            "name": ")" + name + R"(",
            "tiers": [ { "id": "t1", "name": "Bronze", "color": [200, 120, 0, 255] } ],
            "uncategorized": [
                { "id": "s1", "name": "Scenario One", "hash": null,
                  "thresholds": [ { "tierId": "t1", "score": 100.0 } ] }
            ],
            "categories": []
        })";
    }

    class BenchmarkRepositoryTest : public testing::Test {
    protected:
        QTemporaryDir dir;
        std::unique_ptr<BenchmarkRepository> repo;

        void SetUp() override {
            ASSERT_TRUE(dir.isValid());
            repo = std::make_unique<BenchmarkRepository>(dir.path().toStdString());
        }

        QString write(const QString &name, const QByteArray &bytes) const {
            const auto path = QDir(dir.path()).absoluteFilePath(name);
            QFile file(path);
            EXPECT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(bytes);
            return path;
        }
    };

    const LoadedBenchmark *loaded(const BenchmarkFileEntry &entry) {
        return std::get_if<LoadedBenchmark>(&entry.content);
    }
    const ProblemBenchmark *problem(const BenchmarkFileEntry &entry) {
        return std::get_if<ProblemBenchmark>(&entry.content);
    }
}

TEST_F(BenchmarkRepositoryTest, ScanClassifiesAValidTrackableFile) {
    write("bench.json", trackableJson());
    const auto result = repo->scan();
    ASSERT_TRUE(result.snapshot.has_value());
    ASSERT_EQ(result.snapshot->entries.size(), 1U);
    const auto &entry = result.snapshot->entries[0];
    EXPECT_EQ(entry.filename, "bench.json");
    EXPECT_FALSE(entry.digest.empty());
    ASSERT_NE(loaded(entry), nullptr);
    EXPECT_EQ(loaded(entry)->completeness.completeness, domain::Completeness::Trackable);
    EXPECT_EQ(loaded(entry)->benchmark.name, "Bench");
}

TEST_F(BenchmarkRepositoryTest, ScanReportsMissingDirectoryFailureOnceCreationIsImpossible) {
    // Point at a path whose parent is a file, so mkpath cannot succeed.
    write("blocker", QByteArray("x"));
    BenchmarkRepository blocked(QDir(dir.path()).absoluteFilePath("blocker/child").toStdString());
    const auto result = blocked.scan();
    EXPECT_FALSE(result.snapshot.has_value());
    ASSERT_TRUE(result.failure.has_value());
    EXPECT_EQ(*result.failure, BenchmarkScanFailure::DirectoryUnavailable);
}

TEST_F(BenchmarkRepositoryTest, MalformedJsonIsInvalid) {
    write("bad.json", QByteArray("{ not json"));
    const auto entry = repo->scan().snapshot->entries.at(0);
    ASSERT_NE(problem(entry), nullptr);
    EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Invalid);
}

TEST_F(BenchmarkRepositoryTest, NewerSchemaVersionIsUnsupportedAndKeepsName) {
    auto json = trackableJson();
    json.replace("\"schemaVersion\": 1", "\"schemaVersion\": 2");
    write("future.json", json);
    const auto entry = repo->scan().snapshot->entries.at(0);
    ASSERT_NE(problem(entry), nullptr);
    EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Unsupported);
    ASSERT_TRUE(problem(entry)->schemaVersion.has_value());
    EXPECT_EQ(*problem(entry)->schemaVersion, 2);
    ASSERT_TRUE(problem(entry)->displayName.has_value());
    EXPECT_EQ(*problem(entry)->displayName, "Bench");
}

TEST_F(BenchmarkRepositoryTest, DanglingTierReferenceIsInvalid) {
    auto json = trackableJson();
    json.replace("\"tierId\": \"t1\"", "\"tierId\": \"tX\"");  // references a tier that isn't declared
    write("dangling.json", json);
    const auto entry = repo->scan().snapshot->entries.at(0);
    ASSERT_NE(problem(entry), nullptr);
    EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Invalid);
}

TEST_F(BenchmarkRepositoryTest, StructurallyCompleteButMissingThresholdLoadsAsIncomplete) {
    auto json = trackableJson();
    json.replace("\"thresholds\": [ { \"tierId\": \"t1\", \"score\": 100.0 } ]", "\"thresholds\": []");
    write("incomplete.json", json);
    const auto entry = repo->scan().snapshot->entries.at(0);
    ASSERT_NE(loaded(entry), nullptr);
    EXPECT_EQ(loaded(entry)->completeness.completeness, domain::Completeness::Incomplete);
}

TEST_F(BenchmarkRepositoryTest, OneBadFileDoesNotBlockAValidSibling) {
    write("aaa_bad.json", QByteArray("{ broken"));
    write("bbb_good.json", trackableJson());
    const auto entries = repo->scan().snapshot->entries;
    ASSERT_EQ(entries.size(), 2U);
    EXPECT_NE(problem(entries.at(0)), nullptr);  // sorted by name: aaa first
    EXPECT_NE(loaded(entries.at(1)), nullptr);
}

TEST_F(BenchmarkRepositoryTest, DuplicateStableIdWithinAFileIsInvalid) {
    auto json = trackableJson();
    json.replace("\"id\": \"s1\"", "\"id\": \"t1\"");  // scenario reuses the tier's id
    write("dupe.json", json);
    const auto entry = repo->scan().snapshot->entries.at(0);
    ASSERT_NE(problem(entry), nullptr);
    EXPECT_EQ(problem(entry)->problem, BenchmarkFileProblem::Invalid);
}

TEST_F(BenchmarkRepositoryTest, WriteCreatesThenRoundTripsThroughScan) {
    domain::Benchmark def;
    def.id = domain::BenchmarkId{"b1"};
    def.name = "Written";
    def.tiers = {domain::Tier{domain::TierId{"t1"}, "Bronze", {}}};
    def.uncategorized = {domain::ScenarioEntry{domain::ScenarioEntryId{"s1"}, "One", std::nullopt,
                                               {domain::Threshold{domain::TierId{"t1"}, 10.0}}}};

    const auto write = repo->write(def, "written.json", std::nullopt);
    ASSERT_TRUE(write.succeeded());

    const auto entries = repo->scan().snapshot->entries;
    ASSERT_EQ(entries.size(), 1U);
    ASSERT_NE(loaded(entries[0]), nullptr);
    EXPECT_EQ(loaded(entries[0])->benchmark.name, "Written");
    EXPECT_EQ(entries[0].digest, *write.digest);  // scan digest matches the write's returned digest
}

TEST_F(BenchmarkRepositoryTest, ReplaceWithStaleDigestIsAConflictAndDoesNotOverwrite) {
    write("f.json", trackableJson("b1", "Original"));
    const auto original = repo->scan().snapshot->entries.at(0);

    domain::Benchmark edited;
    edited.id = domain::BenchmarkId{"b1"};
    edited.name = "Edited";
    const auto result = repo->write(edited, "f.json", std::string{"deadbeef"});  // wrong digest
    ASSERT_FALSE(result.succeeded());
    EXPECT_EQ(*result.failure, BenchmarkWriteFailure::ExternalModificationConflict);

    const auto after = repo->scan().snapshot->entries.at(0);
    ASSERT_NE(loaded(after), nullptr);
    EXPECT_EQ(loaded(after)->benchmark.name, "Original");  // untouched
    EXPECT_EQ(after.digest, original.digest);
}

TEST_F(BenchmarkRepositoryTest, ReplaceWithMatchingDigestSucceeds) {
    write("f.json", trackableJson("b1", "Original"));
    const auto original = repo->scan().snapshot->entries.at(0);

    domain::Benchmark edited;
    edited.id = domain::BenchmarkId{"b1"};
    edited.name = "Edited";
    edited.tiers = {domain::Tier{domain::TierId{"t1"}, "Bronze", {}}};
    edited.uncategorized = {domain::ScenarioEntry{domain::ScenarioEntryId{"s1"}, "One", std::nullopt,
                                                  {domain::Threshold{domain::TierId{"t1"}, 10.0}}}};
    ASSERT_TRUE(repo->write(edited, "f.json", original.digest).succeeded());
    EXPECT_EQ(loaded(repo->scan().snapshot->entries.at(0))->benchmark.name, "Edited");
}

TEST_F(BenchmarkRepositoryTest, RemoveRequiresMatchingDigest) {
    write("f.json", trackableJson());
    const auto digest = repo->scan().snapshot->entries.at(0).digest;
    EXPECT_EQ(repo->remove("f.json", "wrong").failure, BenchmarkWriteFailure::ExternalModificationConflict);
    EXPECT_TRUE(repo->remove("f.json", digest).ok);
    EXPECT_TRUE(repo->scan().snapshot->entries.empty());
}
