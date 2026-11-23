#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <memory>
#include <string>
#include <variant>

#include "qt_data/benchmark_store.h"

using namespace ksv;
using namespace ksv::data;
using namespace ksv::qt_data;

namespace {
    QByteArray v1Json(const QString &name, const char *tierName = "Bronze") {
        QJsonObject root{
            {"schemaVersion", 1}, {"id", "b1"}, {"name", name},
            {"tiers", QJsonArray{QJsonObject{{"id", "t1"}, {"name", tierName}, {"color", QJsonArray{1, 2, 3, 255}}}}},
            {"uncategorized", QJsonArray{QJsonObject{{"id", "s1"}, {"name", "One"}, {"hash", "abc"},
                                                     {"thresholds", QJsonArray{QJsonObject{{"tierId", "t1"}, {"score", 5}}}}}}},
            {"categories", QJsonArray{QJsonObject{
                {"id", "c1"}, {"name", "Tracking"}, {"color", QJsonArray{4, 5, 6, 255}}, {"scenarios", QJsonArray{}},
                {"subcategories", QJsonArray{QJsonObject{
                    {"id", "sc1"}, {"name", "Smooth"}, {"color", QJsonArray{7, 8, 9, 255}},
                    {"scenarios", QJsonArray{QJsonObject{{"id", "s2"}, {"name", "Two"}, {"hash", QJsonValue::Null},
                                                         {"thresholds", QJsonArray{}}}}}}}}}}},
        };
        return QJsonDocument(root).toJson();
    }

    QByteArray v2Json(const QString &name, const char *tierName = "Bronze") {
        auto root = QJsonDocument::fromJson(v1Json(name, tierName)).object();
        root["schemaVersion"] = 2;
        auto uncategorized = root["uncategorized"].toArray();
        auto entry = uncategorized[0].toObject();
        entry.remove("hash");
        entry["hashes"] = QJsonArray{"abc"};
        uncategorized[0] = entry;
        root["uncategorized"] = uncategorized;
        auto categories = root["categories"].toArray();
        auto category = categories[0].toObject();
        auto subcategories = category["subcategories"].toArray();
        auto sub = subcategories[0].toObject();
        auto scenarios = sub["scenarios"].toArray();
        auto nested = scenarios[0].toObject();
        nested.remove("hash");
        nested["hashes"] = QJsonArray{};
        scenarios[0] = nested;
        sub["scenarios"] = scenarios;
        subcategories[0] = sub;
        category["subcategories"] = subcategories;
        categories[0] = category;
        root["categories"] = categories;
        return QJsonDocument(root).toJson();
    }

    QByteArray v3Json(const char *tierName = "Bronze") {
        auto root = QJsonDocument::fromJson(v2Json("unused", tierName)).object();
        root.remove("id");
        root.remove("name");
        root["schemaVersion"] = 3;
        return QJsonDocument(root).toJson();
    }

    QByteArray withTierReference(const QByteArray &json, const char *tierId) {
        auto copy = json;
        copy.replace("\"tierId\": \"t1\"", QByteArray("\"tierId\": \"") + tierId + "\"");
        return copy;
    }

    const LoadedBenchmark *loaded(const BenchmarkFileEntry &entry) {
        return std::get_if<LoadedBenchmark>(&entry.content);
    }

    std::optional<BenchmarkMigrationFailure> migrationFailure(const BenchmarkFileEntry &entry) {
        const auto *problem = std::get_if<ProblemBenchmark>(&entry.content);
        if (!problem || problem->problem != BenchmarkFileProblem::MigrationFailed) return std::nullopt;
        return problem->migrationFailure;
    }

    class BenchmarkStoreMigrationTest : public testing::Test {
    protected:
        std::unique_ptr<QTemporaryDir> dir;
        std::unique_ptr<BenchmarkStore> store;
        int renamesToFail = 0;
        int writesToFail = 0;
        int renameCalls = 0;

        void SetUp() override { reset(); }

        // A fresh, empty managed folder.
        void reset() {
            dir = std::make_unique<QTemporaryDir>();
            ASSERT_TRUE(dir->isValid());
            auto operations = BenchmarkStore::defaultFileOperations();
            operations.rename = [this, real = operations.rename](const QString &from, const QString &to) {
                ++renameCalls;
                if (renamesToFail == 0) return real(from, to);
                --renamesToFail;
                return false;
            };
            operations.writeAtomically = [this, real = operations.writeAtomically](const QString &path,
                                                                                   const QByteArray &bytes) {
                if (writesToFail == 0) return real(path, bytes);
                --writesToFail;
                return false;
            };
            store = std::make_unique<BenchmarkStore>(dir->path().toStdString(), operations);
        }

        void write(const QString &name, const QByteArray &bytes) const {
            QFile file(QDir(dir->path()).absoluteFilePath(name));
            ASSERT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(bytes);
        }

        [[nodiscard]] std::optional<QByteArray> read(const QString &name) const {
            QFile file(QDir(dir->path()).absoluteFilePath(name));
            if (!file.exists() || !file.open(QIODevice::ReadOnly)) return std::nullopt;
            return file.readAll();
        }

        [[nodiscard]] QStringList listing() const {
            return QDir(dir->path()).entryList({"*.json"}, QDir::Files, QDir::Name);
        }

        [[nodiscard]] std::vector<BenchmarkFileEntry> scan() const { return store->scan().snapshot->entries; }

        [[nodiscard]] BenchmarkFileEntry entryNamed(const std::vector<BenchmarkFileEntry> &entries,
                                                    const QString &filename) const {
            for (const auto &entry: entries)
                if (entry.filename == filename.toStdString()) return entry;
            ADD_FAILURE() << "no entry " << filename.toStdString();
            return {};
        }

        [[nodiscard]] QString tierNameIn(const QString &filename) const {
            const auto root = QJsonDocument::fromJson(read(filename).value_or(QByteArray{})).object();
            return root["tiers"].toArray().at(0).toObject()["name"].toString();
        }

        // The file a single migrating file storing `storedName` ends up as.
        QString migratedAlone(const QString &storedName) {
            reset();
            write("0f3c.json", v2Json(storedName));
            static_cast<void>(scan());
            const auto files = listing();
            return files.size() == 1 ? files.front() : QString("<%1 files>").arg(files.size());
        }
    };
}

TEST_F(BenchmarkStoreMigrationTest, RewritesSchemaOneAndTwoAsSchemaThreeUnderTheirStoredNames) {
    write("1d6a.json", v1Json("Alpha"));
    write("9b2e.json", v2Json("Beta"));

    const auto entries = scan();

    EXPECT_EQ(listing(), (QStringList{"Alpha.json", "Beta.json"}));
    ASSERT_EQ(entries.size(), 2U);
    for (const auto *name: {"Alpha.json", "Beta.json"}) {
        const auto entry = entryNamed(entries, name);
        ASSERT_NE(loaded(entry), nullptr) << name;
        EXPECT_EQ(read(name), std::make_optional(QJsonDocument::fromJson(v3Json()).toJson())) << name;
        const auto &benchmark = loaded(entry)->benchmark;
        EXPECT_EQ(benchmark.tiers.at(0).id.value, "t1");
        EXPECT_EQ(benchmark.uncategorized.at(0).id.value, "s1");
        EXPECT_EQ(benchmark.uncategorized.at(0).hashes, std::vector<std::string>{"abc"});
        EXPECT_EQ(benchmark.categories.at(0).subcategories.at(0).scenarios.at(0).id.value, "s2");
        EXPECT_TRUE(benchmark.categories.at(0).subcategories.at(0).scenarios.at(0).hashes.empty());
    }
    // The digest reported is the one the rewritten bytes have.
    const auto again = scan();
    EXPECT_EQ(entryNamed(again, "Alpha.json").digest, entryNamed(entries, "Alpha.json").digest);
}

TEST_F(BenchmarkStoreMigrationTest, AnUpgradedResultTheReaderRejectsIsNeverWritten) {
    const auto dangling = withTierReference(v2Json("Alpha"), "tX");
    auto duplicated = v2Json("Beta");
    duplicated.replace("\"id\": \"s2\"", "\"id\": \"s1\"");
    write("aaaa.json", dangling);
    write("bbbb.json", duplicated);

    const auto entries = scan();

    for (const auto &[name, bytes]: {std::pair{"aaaa.json", dangling}, std::pair{"bbbb.json", duplicated}}) {
        EXPECT_EQ(migrationFailure(entryNamed(entries, name)),
                  std::make_optional(BenchmarkMigrationFailure{2, BenchmarkMigrationFailureReason::InvalidResult}))
            << name;
        EXPECT_EQ(read(name), std::make_optional(bytes)) << name;
    }
    EXPECT_EQ(listing(), (QStringList{"aaaa.json", "bbbb.json"}));
}

TEST_F(BenchmarkStoreMigrationTest, MalformedOlderFilesFailAtTheirStepAndAreLeftAsTheyWere) {
    auto badV1 = QJsonDocument::fromJson(v1Json("Alpha")).object();
    badV1.remove("name");
    auto badV2 = QJsonDocument::fromJson(v2Json("Beta")).object();
    badV2["id"] = "";
    const auto v1Bytes = QJsonDocument(badV1).toJson();
    const auto v2Bytes = QJsonDocument(badV2).toJson();
    write("one.json", v1Bytes);
    write("two.json", v2Bytes);

    const auto entries = scan();

    EXPECT_EQ(migrationFailure(entryNamed(entries, "one.json")),
              std::make_optional(BenchmarkMigrationFailure{1, BenchmarkMigrationFailureReason::SourceMismatch}));
    EXPECT_EQ(migrationFailure(entryNamed(entries, "two.json")),
              std::make_optional(BenchmarkMigrationFailure{2, BenchmarkMigrationFailureReason::SourceMismatch}));
    EXPECT_EQ(read("one.json"), std::make_optional(v1Bytes));
    EXPECT_EQ(read("two.json"), std::make_optional(v2Bytes));

    write("one.json", v1Json("Alpha"));
    const auto retried = scan();
    EXPECT_NE(loaded(entryNamed(retried, "Alpha.json")), nullptr);
}

TEST_F(BenchmarkStoreMigrationTest, ARenameFailureLeavesTheOriginalUnderItsOldName) {
    write("1d6a.json", v1Json("Alpha"));
    renamesToFail = 1;

    const auto entries = scan();

    EXPECT_EQ(migrationFailure(entryNamed(entries, "1d6a.json")),
              std::make_optional(BenchmarkMigrationFailure{2, BenchmarkMigrationFailureReason::RenameFailed}));
    EXPECT_EQ(listing(), (QStringList{"1d6a.json"}));
    EXPECT_EQ(read("1d6a.json"), std::make_optional(v1Json("Alpha")));

    const auto retried = scan();
    EXPECT_NE(loaded(entryNamed(retried, "Alpha.json")), nullptr);
    EXPECT_EQ(listing(), (QStringList{"Alpha.json"}));
}

TEST_F(BenchmarkStoreMigrationTest, AWriteFailureLeavesTheOriginalUnderItsNewNameAndIsRewrittenInPlaceLater) {
    write("1d6a.json", v1Json("Alpha"));
    writesToFail = 1;

    const auto entries = scan();

    EXPECT_EQ(migrationFailure(entryNamed(entries, "Alpha.json")),
              std::make_optional(BenchmarkMigrationFailure{2, BenchmarkMigrationFailureReason::WriteFailed}));
    EXPECT_EQ(listing(), (QStringList{"Alpha.json"}));
    EXPECT_EQ(read("Alpha.json"), std::make_optional(v1Json("Alpha")));

    renameCalls = 0;
    const auto retried = scan();
    EXPECT_NE(loaded(entryNamed(retried, "Alpha.json")), nullptr);
    EXPECT_EQ(renameCalls, 0);
    EXPECT_EQ(QJsonDocument::fromJson(*read("Alpha.json")).object()["schemaVersion"].toInt(), 3);
}

TEST_F(BenchmarkStoreMigrationTest, DerivesFilenamesFromStoredNamesThatAreNotUsableAsTheyAre) {
    EXPECT_EQ(migratedAlone("Voltaic <S5>: \"Hard\""), "Voltaic S5 Hard.json");
    EXPECT_EQ(migratedAlone("Voltaic. . "), "Voltaic.json");
    EXPECT_EQ(migratedAlone(""), "Benchmark.json");
    EXPECT_EQ(migratedAlone("CON"), "Benchmark.json");
    EXPECT_EQ(migratedAlone(QString(300, 'a')), QString(250, 'a') + ".json");
}

TEST_F(BenchmarkStoreMigrationTest, ANameHeldByAFileThatIsNotMigratingGetsASuffix) {
    write("Alpha.json", v3Json("Kept"));
    write("1d6a.json", v2Json("Alpha", "Moved"));

    static_cast<void>(scan());

    EXPECT_EQ(listing(), (QStringList{"Alpha (2).json", "Alpha.json"}));
    EXPECT_EQ(tierNameIn("Alpha.json"), "Kept");
    EXPECT_EQ(tierNameIn("Alpha (2).json"), "Moved");
}

TEST_F(BenchmarkStoreMigrationTest, MigratingFilesClaimNamesInFoldedFilenameOrder) {
    write("B-second.json", v2Json("X", "Second"));
    write("a-first.json", v2Json("X", "First"));

    static_cast<void>(scan());

    EXPECT_EQ(tierNameIn("X.json"), "First");
    EXPECT_EQ(tierNameIn("X (2).json"), "Second");
}

TEST_F(BenchmarkStoreMigrationTest, SeveralEmptyNamesFallBackToNumberedBenchmarks) {
    write("1.json", v2Json(""));
    write("2.json", v1Json(""));
    write("3.json", v2Json(""));

    static_cast<void>(scan());

    EXPECT_EQ(listing(), (QStringList{"Benchmark (2).json", "Benchmark (3).json", "Benchmark.json"}));
}

TEST_F(BenchmarkStoreMigrationTest, ANameAnotherMigratingFileIsLeavingIsNotReused) {
    write("a.json", v2Json("b", "From a"));
    write("b.json", v2Json("c", "From b"));

    static_cast<void>(scan());

    EXPECT_EQ(listing(), (QStringList{"b (2).json", "c.json"}));
    EXPECT_EQ(tierNameIn("b (2).json"), "From a");
    EXPECT_EQ(tierNameIn("c.json"), "From b");
}

TEST_F(BenchmarkStoreMigrationTest, AFileAlreadyNamedAfterItsStoredNameIsRewrittenWithoutARename) {
    write("Alpha.json", v2Json("Alpha"));

    const auto entries = scan();

    EXPECT_EQ(renameCalls, 0);
    EXPECT_NE(loaded(entryNamed(entries, "Alpha.json")), nullptr);
    EXPECT_EQ(read("Alpha.json"), std::make_optional(QJsonDocument::fromJson(v3Json()).toJson()));
}

TEST_F(BenchmarkStoreMigrationTest, AFileWhoseNameDiffersOnlyInCaseTakesTheStoredSpelling) {
    write("alpha.json", v2Json("Alpha"));

    const auto entries = scan();

    EXPECT_EQ(listing(), (QStringList{"Alpha.json"}));
    EXPECT_NE(loaded(entryNamed(entries, "Alpha.json")), nullptr);
}

TEST_F(BenchmarkStoreMigrationTest, CollisionsIgnoreLetterCaseBeyondAscii) {
    write("alpha.json", v3Json("Kept"));
    write(QString::fromUtf8("\xC3\x84rger.json"), v3Json("Kept"));
    write("1d6a.json", v2Json("ALPHA"));
    write("9b2e.json", v2Json(QString::fromUtf8("\xC3\xA4RGER")));

    static_cast<void>(scan());

    EXPECT_TRUE(listing().contains("ALPHA (2).json"));
    EXPECT_TRUE(listing().contains(QString::fromUtf8("\xC3\xA4RGER (2).json")));
}

TEST_F(BenchmarkStoreMigrationTest, InvalidAndUnsupportedFilesAreLeftByteIdentical) {
    const QByteArray broken("{ broken");
    auto future = v3Json();
    future.replace("\"schemaVersion\": 3", "\"schemaVersion\": 4");
    write("broken.json", broken);
    write("future.json", future);

    static_cast<void>(scan());

    EXPECT_EQ(listing(), (QStringList{"broken.json", "future.json"}));
    EXPECT_EQ(read("broken.json"), std::make_optional(broken));
    EXPECT_EQ(read("future.json"), std::make_optional(future));
    EXPECT_EQ(renameCalls, 0);
}
