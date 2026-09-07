#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <optional>
#include <string>

#include "app/app.h"
#include "formats/protobuf/proto_decoder.h"
#include "qt_data/benchmark_repository.h"
#include "qt_data/series_config_store.h"
#include "fake_settings_service.h"

using namespace ksv;
using namespace ksv::application;

namespace {
    QByteArray trackableJson() {
        return R"({
            "schemaVersion": 1, "id": "b1", "name": "Composed",
            "tiers": [ { "id": "t1", "name": "Bronze", "color": [200,120,0,255] } ],
            "uncategorized": [ { "id": "s1", "name": "One", "hash": null,
                "thresholds": [ { "tierId": "t1", "score": 100.0 } ] } ],
            "categories": []
        })";
    }
}

TEST(BenchmarkComposition, RealGraphLoadsManagedBenchmarksAtStartup) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile file(QDir(dir.path()).absoluteFilePath("composed.json"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(trackableJson());
    file.close();

    auto settings = std::make_shared<tests_support::FakeSettingsService>();
    auto repo = std::make_shared<qt_data::BenchmarkRepository>(dir.path().toStdString());
    App app(settings, std::make_shared<data::ProtoDecoder>(),
            std::make_shared<qt_data::SeriesConfigStore>(settings), nullptr, repo);

    const auto service = app.benchmarkLibraryService();
    ASSERT_NE(service, nullptr);
    ASSERT_TRUE(service->snapshot().has_value());
    ASSERT_EQ(service->snapshot()->entries.size(), 1U);
    const auto *loaded = std::get_if<LoadedBenchmark>(&service->snapshot()->entries[0].content);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->benchmark.name, "Composed");
}

namespace {
    // A valid, editable document for the given embedded id, used to simulate a hand edit
    // of a managed file on disk.
    QByteArray externallyEditedJson(const std::string &id) {
        return R"({
            "schemaVersion": 1, "id": ")" + QByteArray::fromStdString(id) + R"(", "name": "Externally Edited",
            "tiers": [],
            "uncategorized": [
                { "id": "s1", "name": "One", "hash": null, "thresholds": [] }
            ],
            "categories": []
        })";
    }
}

TEST(BenchmarkComposition, AuthoringRoundTripImportsSavesReopensAndDeletes) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    auto settings = std::make_shared<tests_support::FakeSettingsService>();
    auto repo = std::make_shared<qt_data::BenchmarkRepository>(dir.path().toStdString());
    App app(settings, std::make_shared<data::ProtoDecoder>(),
            std::make_shared<qt_data::SeriesConfigStore>(settings), nullptr, repo);

    auto service = app.benchmarkLibraryService();
    ASSERT_NE(service, nullptr);
    ASSERT_NE(app.benchmarkManagerVm(), nullptr);

    const auto import = service->importPlaylist(
        QDir(TEST_FILES_DIR).absoluteFilePath("Viscose Benchmarks Beta - Intermediate.json").toStdString());
    ASSERT_TRUE(import.ok());
    EXPECT_TRUE(import.skippedDuplicateIndices.empty());
    ASSERT_TRUE(service->hasDraft());
    ASSERT_TRUE(service->draftDirty());
    const auto draft = service->draft();
    ASSERT_TRUE(draft.has_value());
    EXPECT_EQ(draft->name, "Viscose Benchmarks Beta - Intermediate");
    ASSERT_EQ(draft->uncategorized.size(), 36U);
    EXPECT_EQ(draft->uncategorized.front().name, "WhisphereRawControl");
    EXPECT_EQ(draft->uncategorized.back().name, "voxTargetClick 20% Small");
    const domain::BenchmarkId benchmarkId = draft->id;

    const auto savedPath =
        QDir(dir.path()).absoluteFilePath(QString::fromStdString(benchmarkId.value) + ".json");
    ASSERT_TRUE(service->renameBenchmark("Viscose Intermediate").ok());
    const auto save = service->saveDraft();
    ASSERT_TRUE(save.ok());
    ASSERT_TRUE(QFile::exists(savedPath));
    ASSERT_TRUE(service->snapshot().has_value());
    ASSERT_EQ(service->snapshot()->entries.size(), 1U);
    EXPECT_EQ(service->snapshot()->entries.front().filename,
              benchmarkId.value + ".json");

    service->discardDraft();
    ASSERT_TRUE(service->openDraft(benchmarkId));
    const auto reopened = service->draft();
    ASSERT_TRUE(reopened.has_value());
    ASSERT_EQ(reopened->uncategorized.size(), 36U);
    EXPECT_EQ(reopened->uncategorized.front().name, "WhisphereRawControl");
    EXPECT_EQ(reopened->uncategorized.back().name, "voxTargetClick 20% Small");
    EXPECT_FALSE(service->draftDirty());

    // A hand edit on disk makes the stale-digest save a conflict that leaves the file intact.
    {
        QFile file(savedPath);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(externallyEditedJson(benchmarkId.value));
    }
    ASSERT_TRUE(service->renameBenchmark("Renamed After External Edit").ok());
    const auto conflicted = service->saveDraft();
    EXPECT_EQ(conflicted.error, std::make_optional(BenchmarkSaveError::Conflict));
    {
        QFile file(savedPath);
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        EXPECT_EQ(file.readAll(), externallyEditedJson(benchmarkId.value));
    }

    // Refresh is suppressed while a dirty draft is open, so the pending rename is discarded
    // first; refresh then admits the external edit and the reopened definition is deletable.
    service->discardDraft();
    service->refresh();
    ASSERT_TRUE(service->openDraft(benchmarkId));
    ASSERT_TRUE(service->draft().has_value());
    EXPECT_EQ(service->draft()->name, "Externally Edited");

    const auto deleted = service->deleteBenchmark(benchmarkId);
    ASSERT_TRUE(deleted.ok());
    EXPECT_FALSE(QFile::exists(savedPath));
    ASSERT_TRUE(service->snapshot().has_value());
    EXPECT_TRUE(service->snapshot()->entries.empty());
    EXPECT_FALSE(service->hasDraft());
}
