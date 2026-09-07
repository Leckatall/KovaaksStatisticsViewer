#pragma once

#include <optional>
#include <string>
#include <vector>

// Playlist payload shared by IPlaylistReader (data-layer port) and
// IBenchmarkLibraryService (application contract). It lives in ksv_contracts so the
// service contract does not have to include the data-layer port header to name it,
// mirroring benchmark_library_snapshot.h.
namespace ksv::application {
    struct PlaylistSeed {
        std::optional<std::string> name;         // usable playlist name when present
        std::vector<std::string> scenarioNames;  // first occurrence of each, in source order
        std::vector<int> skippedDuplicateIndices; // 0-based source positions dropped as duplicates
    };

    enum class PlaylistImportFailure { FileUnreadable, MalformedJson, NoUsableScenarioList };

    struct PlaylistReadResult {
        std::optional<PlaylistSeed> seed;
        std::optional<PlaylistImportFailure> failure;
    };
}
