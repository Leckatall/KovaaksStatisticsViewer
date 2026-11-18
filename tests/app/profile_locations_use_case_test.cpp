#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "fake_file_service.h"
#include "fake_profile_service.h"
#include "fake_settings_service.h"
#include "usecases/profile_locations_use_case.h"

using namespace ksv::application;
using namespace ksv::tests_support;

namespace {
    using Dirs = std::vector<std::string>;

    struct Fixture {
        std::shared_ptr<FakeSettingsService> settings = std::make_shared<FakeSettingsService>();
        std::shared_ptr<FakeFileService> files = std::make_shared<FakeFileService>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        std::unique_ptr<ProfileLocationsUseCase> locations;
        int dir_changes = 0;
        int path_changes = 0;
        int profile_changes = 0;

        void build(const Dirs &dirs = {"C:/Kovaaks"}) {
            settings->dirs = dirs;
            settings->profile_path = "C:/Profile/profile.pb";
            locations = std::make_unique<ProfileLocationsUseCase>(settings, files, profile);
            locations->onKovaaksDirChanged([this] { ++dir_changes; });
            locations->onProfilePathChanged([this] { ++path_changes; });
            locations->onProfileChanged([this] { ++profile_changes; });
        }
    };
}

TEST(ProfileLocationsUseCase, KovaaksDirIsTheFirstConfiguredRoot) {
    Fixture fixture;
    fixture.build({"C:/Primary", "D:/Secondary"});
    EXPECT_EQ(fixture.locations->kovaaksDir(), "C:/Primary");
}

TEST(ProfileLocationsUseCase, KovaaksDirIsEmptyWhenNoneIsConfigured) {
    Fixture fixture;
    fixture.build({});
    EXPECT_EQ(fixture.locations->kovaaksDir(), "");
}

TEST(ProfileLocationsUseCase, SetKovaaksDirOnAnEmptyListAddsIt) {
    Fixture fixture;
    fixture.build({});

    fixture.locations->setKovaaksDir("E:/New");

    EXPECT_EQ(fixture.settings->dirs, (Dirs{"E:/New"}));
    EXPECT_EQ(fixture.dir_changes, 1);
}

TEST(ProfileLocationsUseCase, SetKovaaksDirReplacesOnlyTheFirstRoot) {
    Fixture fixture;
    fixture.build({"C:/Primary", "D:/Secondary"});

    fixture.locations->setKovaaksDir("E:/Replacement");

    EXPECT_EQ(fixture.settings->dirs, (Dirs{"E:/Replacement", "D:/Secondary"}));
    EXPECT_EQ(fixture.dir_changes, 1);
}

TEST(ProfileLocationsUseCase, SetKovaaksDirToTheCurrentRootWritesNothing) {
    Fixture fixture;
    fixture.build({"C:/Primary", "D:/Secondary"});
    int writes = 0;
    fixture.settings->onKovaaksDirsChanged([&writes] { ++writes; });

    fixture.locations->setKovaaksDir("C:/Primary");

    EXPECT_EQ(writes, 0);
    EXPECT_EQ(fixture.dir_changes, 0);
}

TEST(ProfileLocationsUseCase, ARootChangeMadeElsewhereIsPublished) {
    Fixture fixture;
    fixture.build();

    fixture.settings->setKovaaksDirs({"F:/Elsewhere"});

    EXPECT_EQ(fixture.locations->kovaaksDir(), "F:/Elsewhere");
    EXPECT_EQ(fixture.dir_changes, 1);
}

// A KovaaK's directory chosen before its first run has no performances folder yet; once KovaaK's
// writes there, the file watcher reports new sources and the directory has to read as valid.
TEST(ProfileLocationsUseCase, ArrivingSourcesThatMakeTheDirValidArePublished) {
    Fixture fixture;
    fixture.files->source_roots_valid = false;
    fixture.build();
    ASSERT_FALSE(fixture.locations->isKovaaksDirValid());

    fixture.files->source_roots_valid = true;
    fixture.files->notifySourcesChanged();

    EXPECT_TRUE(fixture.locations->isKovaaksDirValid());
    EXPECT_EQ(fixture.dir_changes, 1);
}

TEST(ProfileLocationsUseCase, ArrivingSourcesThatLeaveValidityAloneAreNotPublished) {
    Fixture fixture;
    fixture.build();

    fixture.files->notifySourcesChanged();

    EXPECT_EQ(fixture.dir_changes, 0);
}

TEST(ProfileLocationsUseCase, ValidityAfterARootChangeIsTheBaselineForLaterSourceChanges) {
    Fixture fixture;
    fixture.build();

    fixture.files->source_roots_valid = false;
    fixture.locations->setKovaaksDir("E:/Empty");
    ASSERT_EQ(fixture.dir_changes, 1);
    fixture.files->notifySourcesChanged();

    EXPECT_EQ(fixture.dir_changes, 1);
}

TEST(ProfileLocationsUseCase, SetProfilePathWritesAndPublishes) {
    Fixture fixture;
    fixture.build();

    fixture.locations->setProfilePath("D:/Other/profile.pb");

    EXPECT_EQ(fixture.settings->profile_path, "D:/Other/profile.pb");
    EXPECT_EQ(fixture.locations->profilePath(), "D:/Other/profile.pb");
    EXPECT_EQ(fixture.path_changes, 1);
}

TEST(ProfileLocationsUseCase, SetProfilePathToTheCurrentPathWritesNothing) {
    Fixture fixture;
    fixture.build();

    fixture.locations->setProfilePath("C:/Profile/profile.pb");

    EXPECT_EQ(fixture.path_changes, 0);
}

TEST(ProfileLocationsUseCase, ProfileChangesArePublished) {
    Fixture fixture;
    fixture.build();

    fixture.profile->profile_loaded = true;
    fixture.profile->notifyProfileChanged();

    EXPECT_TRUE(fixture.locations->isProfileLoaded());
    EXPECT_EQ(fixture.profile_changes, 1);
}

TEST(ProfileLocationsUseCase, NothingIsPublishedAfterItIsDestroyed) {
    Fixture fixture;
    fixture.build();
    fixture.locations.reset();

    fixture.settings->setKovaaksDirs({"E:/After"});
    fixture.settings->setProfilePath("E:/After/profile.pb");
    fixture.profile->notifyProfileChanged();

    EXPECT_EQ(fixture.dir_changes, 0);
    EXPECT_EQ(fixture.path_changes, 0);
    EXPECT_EQ(fixture.profile_changes, 0);
}
