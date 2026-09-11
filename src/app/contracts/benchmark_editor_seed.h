#pragma once

#include <optional>
#include <vector>

#include "benchmarks/benchmark.h"
#include "data/interfaces/i_benchmarks_service.h"
#include "playlist_seed.h"

namespace ksv::application {
    // What presentation needs to start an editing session for one benchmark: a complete working
    // value plus, when opened from an accepted entry, the admitted filename/digest token the next
    // Save carries. `token` is std::nullopt for a brand-new or freshly imported seed that has never
    // been persisted.
    struct BenchmarkEditorSeed {
        domain::Benchmark benchmark;
        std::optional<data::BenchmarkEditToken> token;
    };

    // Result of importPlaylistSeed: a seed on success, or the existing playlist-import failure
    // classification. `skippedDuplicateIndices` is populated on success, mirroring the old
    // PlaylistImportResult.
    struct PlaylistSeedImport {
        std::optional<PlaylistImportFailure> failure;
        std::optional<BenchmarkEditorSeed> seed;
        std::vector<int> skippedDuplicateIndices;
        [[nodiscard]] bool ok() const { return !failure.has_value() && seed.has_value(); }
    };
}
