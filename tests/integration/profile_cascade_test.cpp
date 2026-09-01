//
// The profile subsystem wired for real: SettingsService -> ProfileService ->
// FileService/ProtoDecoder/ProfileSerializer. Covers the setProfilePath ->
// applyProfilePath -> loadProfile -> onProfileChanged cascade, the store
// save/reload round-trip, and version-mismatch regeneration through the
// live service (not the serializer in isolation).
//

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <filesystem>
#include <fstream>
#include <memory>
#include <set>

#include "file_service.h"
#include "formats/protobuf/profile_serializer.h"
#include "formats/protobuf/proto_decoder.h"
#include "profile_service.h"
#include "run_ingestor.h"

#include "integration_env.h"
#include "profile_store_files.h"

using namespace ksv;
using ksv::tests_support::quarantineFiles;
using ksv::tests_support::readFile;

namespace {
    // The 1wall6targets fixture's score total. Its perf and its paired CSV happen to
    // agree on score to well within this test's tolerance, so decoding the perf here to
    // derive the expectation -- as this test used to -- discriminated nothing: it is the
    // shots/hits/misses/kills assertions above that prove the CSV totals won.
    constexpr float kPairedRunScore = 153.088882F;

    class ProfileCascadeTest : public testing::Test {
    protected:
        integration::TestEnv env;
        integration::ProfileStack stack;
        std::shared_ptr<data::ProfileSerializer> serializer;
        std::shared_ptr<data::ProfileService> profileService;

        void SetUp() override {
            ASSERT_TRUE(env.valid());
            ASSERT_TRUE(env.makePerformancesDir());
            ASSERT_FALSE(env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty());

            stack = env.makeProfileStack();
            serializer = stack.serializer;
            profileService = stack.profileService;
        }
    };

    TEST_F(ProfileCascadeTest, ChangingProfilePathReloadsAndNotifiesObservers) {
        int notify_count = 0;
        profileService->onProfileChanged([&] { ++notify_count; });

        const QString newPath = QDir(env.rootPath()).absoluteFilePath("relocated/profile.pb");
        env.settings->setProfilePath(newPath.toStdString());

        EXPECT_GE(notify_count, 1);
        EXPECT_TRUE(profileService->isProfileLoaded());
        EXPECT_FALSE(profileService->getScenarioList().empty());
    }

    TEST_F(ProfileCascadeTest, GeneratingProfileWritesStoreThatReloadsWithoutRescanning) {
        profileService->loadProfile();
        const auto originalCount = profileService->getScenarioList().size();
        ASSERT_GT(originalCount, 0u);
        ASSERT_TRUE(std::filesystem::exists(env.profileStorePath().toStdString()));

        // Wipe the source .perf files: a fresh service that still returns scenarios
        // must have loaded them from the store, not re-scanned the (now empty) dir.
        ASSERT_TRUE(QDir(env.performancesDir()).removeRecursively());

        const auto reloaded = env.makeProfileStack().profileService;
        reloaded->loadProfile();

        EXPECT_EQ(reloaded->getScenarioList().size(), originalCount);
    }

    TEST_F(ProfileCascadeTest, MismatchedStoreVersionRebuildsFromDirectory) {
        // Plant a store stamped with an incompatible version at the configured path.
        profile::File stale;
        stale.mutable_header()->set_version(1);
        auto *run = stale.mutable_store()->add_runs();
        run->mutable_scenario_id()->set_name("Should Not Appear");
        run->mutable_scenario_id()->set_hash("stale-hash");
        run->set_start_time(1);
        {
            std::ofstream out(env.profileStorePath().toStdString(),
                              std::ios::out | std::ios::binary | std::ios::trunc);
            stale.SerializeToOstream(&out);
        }
        const auto profilePath = std::filesystem::path(env.profileStorePath().toStdString());
        const auto rejectedBytes = readFile(profilePath);

        profileService->loadProfile();

        EXPECT_TRUE(profileService->isProfileLoaded());
        EXPECT_TRUE(std::filesystem::exists(profilePath));
        const auto quarantined = quarantineFiles(profilePath, "version-mismatch");
        ASSERT_EQ(quarantined.size(), 1);
        EXPECT_EQ(readFile(quarantined.front()), rejectedBytes);
        EXPECT_NE(readFile(profilePath), rejectedBytes);
        const auto scenarios = profileService->getScenarioList();
        EXPECT_FALSE(scenarios.empty());
        for (const auto &s: scenarios) {
            EXPECT_NE(s.name, "Should Not Appear") << "rejected store leaked into the rebuilt profile";
        }
    }

    TEST_F(ProfileCascadeTest, GeneratesAndReloadsRunsFromTwoSourceRoots) {
        QTemporaryDir second_root;
        ASSERT_TRUE(second_root.isValid());
        const auto second_performances = QDir(second_root.path()).absoluteFilePath("FPSAimTrainer/performances");
        ASSERT_TRUE(QDir().mkpath(second_performances));
        ASSERT_TRUE(QFile::copy(integration::fixturePath("VT FlyTS Novice S5.perf"),
                               QDir(second_performances).absoluteFilePath("VT FlyTS Novice S5.perf")));
        env.settings->setKovaaksDirs({env.rootPath().toStdString(), second_root.path().toStdString()});

        profileService->generateProfileFromDirectory();
        const auto stored = serializer->load(env.profileStorePath().toStdString());

        ASSERT_TRUE(std::holds_alternative<domain::UserProfile>(stored));
        const auto& profile = std::get<domain::UserProfile>(stored);
        ASSERT_EQ(profile.getAllRunRecords().size(), 2);
        std::set<std::string> resolved_roots;
        for (const auto &run : profile.getAllRunRecords()) {
            const auto resolved = run.sources.perf ? profile.sources().resolve(*run.sources.perf) : std::nullopt;
            ASSERT_TRUE(resolved.has_value());
            resolved_roots.insert(std::filesystem::path(*resolved).parent_path().parent_path().parent_path()
                                      .generic_string());
        }
        EXPECT_EQ(resolved_roots,
                  (std::set<std::string>{QDir::fromNativeSeparators(env.rootPath()).toStdString(),
                                         QDir::fromNativeSeparators(second_root.path()).toStdString()}));
    }

    TEST_F(ProfileCascadeTest, PairedPerfAndCsvFixtureProduceOneRunWithCanonicalCsvTotals) {
        ASSERT_TRUE(env.makeStatsDir());
        ASSERT_FALSE(env.copyFixtureIntoStats("1wall6targets TE.csv").isEmpty());

        profileService->generateProfileFromDirectory();
        const auto stored = serializer->load(env.profileStorePath().toStdString());
        ASSERT_TRUE(std::holds_alternative<domain::UserProfile>(stored));
        const auto &profile = std::get<domain::UserProfile>(stored);

        const domain::ScenarioId scenario{.name = "1wall6targets TE", .hash = "3e50391f3c3f484c10a4b8fb362ded17"};
        const auto runs = profile.getRunsForScenario(scenario);
        ASSERT_EQ(runs.size(), 1U) << "a paired perf+CSV must fold into one run, not two";
        const auto &run = runs.front();

        EXPECT_EQ(run.totals().shots, 180);
        EXPECT_EQ(run.totals().hits, 166);
        EXPECT_EQ(run.totals().misses, 14);
        EXPECT_EQ(run.totals().kills, 166);
        EXPECT_NEAR(run.totals().score, kPairedRunScore, 1e-4F);
        ASSERT_TRUE(run.performance.has_value());
        EXPECT_FALSE(run.performance->samples.empty());
        ASSERT_TRUE(run.stats.has_value());
        EXPECT_EQ(run.stats->sens_scale, "Valorant");
        EXPECT_TRUE(run.sources.perf.has_value());
        EXPECT_TRUE(run.sources.csv.has_value());
    }

    TEST_F(ProfileCascadeTest, CsvOnlyTimestampedFixtureAppearsInReloadedScenarioHistory) {
        ASSERT_TRUE(QFile::remove(QDir(env.performancesDir()).absoluteFilePath("1wall6targets TE.perf")));
        ASSERT_TRUE(env.makeStatsDir());
        ASSERT_FALSE(env.copyFixtureIntoStats(
            "1wall6targets TE.csv",
            "1wall6targets TE - Challenge - 2026.08.27-02.26.40 Stats.csv").isEmpty());

        profileService->generateProfileFromDirectory();
        const auto stored = serializer->load(env.profileStorePath().toStdString());
        ASSERT_TRUE(std::holds_alternative<domain::UserProfile>(stored));
        const auto &profile = std::get<domain::UserProfile>(stored);

        const domain::ScenarioId scenario{.name = "1wall6targets TE", .hash = "3e50391f3c3f484c10a4b8fb362ded17"};
        const auto runs = profile.getRunsForScenario(scenario);
        ASSERT_EQ(runs.size(), 1U);
        const auto &run = runs.front();

        EXPECT_FALSE(run.performance.has_value());
        EXPECT_FALSE(run.sources.perf.has_value());
        ASSERT_TRUE(run.sources.csv.has_value());
        ASSERT_TRUE(run.stats.has_value());
        EXPECT_EQ(run.totals().shots, 180);
        EXPECT_NEAR(run.scenario_length, 59.852F, 0.05F);
    }
}
