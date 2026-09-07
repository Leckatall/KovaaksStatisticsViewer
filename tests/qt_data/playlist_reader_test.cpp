#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "qt_data/playlist_reader.h"

using namespace ksv::application;
using namespace ksv::qt_data;

namespace {
    class PlaylistReaderTest : public testing::Test {
    protected:
        QTemporaryDir dir;

        void SetUp() override { ASSERT_TRUE(dir.isValid()); }

        QString writeTemp(const QByteArray &content) const {
            const auto path = QDir(dir.path()).absoluteFilePath("playlist.json");
            QFile file(path);
            EXPECT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(content);
            return path;
        }
    };
}

TEST(PlaylistReader, DecodesNameAndOrderedScenariosFromRealExport) {
    PlaylistReader reader;
    const auto result = reader.read(QDir(TEST_FILES_DIR).absoluteFilePath(
        "Viscose Benchmarks Beta - Intermediate.json").toStdString());
    ASSERT_TRUE(result.seed.has_value());
    EXPECT_EQ(result.seed->name, std::optional<std::string>{"Viscose Benchmarks Beta - Intermediate"});
    ASSERT_EQ(result.seed->scenarioNames.size(), 36U);
    EXPECT_EQ(result.seed->scenarioNames.front(), "WhisphereRawControl");
    EXPECT_EQ(result.seed->scenarioNames.back(), "voxTargetClick 20% Small");
    EXPECT_TRUE(result.seed->skippedDuplicateIndices.empty());  // the real export has no duplicates
}

TEST_F(PlaylistReaderTest, CollapsesDuplicateScenarioNamesPreservingFirstOccurrence) {
    const auto path = writeTemp(R"({"playlistName":"P","scenarioList":[
        {"scenario_name":"A","play_Count":1},
        {"scenario_name":"B","play_Count":1},
        {"scenario_name":"A","play_Count":1}]})");

    const auto result = PlaylistReader{}.read(path.toStdString());

    ASSERT_TRUE(result.seed.has_value());
    EXPECT_EQ(result.seed->scenarioNames, (std::vector<std::string>{"A", "B"}));
    EXPECT_EQ(result.seed->skippedDuplicateIndices, (std::vector<int>{2}));
}

TEST(PlaylistReader, MissingFileFailsFileUnreadable) {
    EXPECT_EQ(PlaylistReader{}.read("does/not/exist.json").failure,
              std::make_optional(PlaylistImportFailure::FileUnreadable));
}

TEST_F(PlaylistReaderTest, MalformedJsonFailsMalformedJson) {
    const auto path = writeTemp("not json {");
    EXPECT_EQ(PlaylistReader{}.read(path.toStdString()).failure,
              std::make_optional(PlaylistImportFailure::MalformedJson));
}

TEST_F(PlaylistReaderTest, EmptyOrAbsentScenarioListFailsNoUsableScenarioList) {
    const auto empty = writeTemp(R"({"playlistName":"P","scenarioList":[]})");
    EXPECT_EQ(PlaylistReader{}.read(empty.toStdString()).failure,
              std::make_optional(PlaylistImportFailure::NoUsableScenarioList));
    const auto absent = writeTemp(R"({"playlistName":"P"})");
    EXPECT_EQ(PlaylistReader{}.read(absent.toStdString()).failure,
              std::make_optional(PlaylistImportFailure::NoUsableScenarioList));
}

TEST_F(PlaylistReaderTest, BlankAndNonObjectScenarioEntriesAreSkipped) {
    const auto path = writeTemp(R"({"playlistName":"P","scenarioList":[
        {"scenario_name":"  ","play_Count":1},
        42,
        {"play_Count":1},
        {"scenario_name":"A","play_Count":1}]})");

    const auto result = PlaylistReader{}.read(path.toStdString());

    ASSERT_TRUE(result.seed.has_value());
    EXPECT_EQ(result.seed->scenarioNames, (std::vector<std::string>{"A"}));
    EXPECT_TRUE(result.seed->skippedDuplicateIndices.empty());
}

TEST_F(PlaylistReaderTest, AbsentPlaylistNameLeavesSeedNameEmptyButStillDecodes) {
    const auto path = writeTemp(R"({"scenarioList":[{"scenario_name":"A","play_Count":1}]})");

    const auto result = PlaylistReader{}.read(path.toStdString());

    ASSERT_TRUE(result.seed.has_value());
    EXPECT_FALSE(result.seed->name.has_value());
    EXPECT_EQ(result.seed->scenarioNames, (std::vector<std::string>{"A"}));
}
