#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "benchmark_library_snapshot.h"
#include "benchmarks/benchmark_resolution.h"
#include "playlist_seed.h"
#include "run.h"

namespace ksv::application {
    enum class BenchmarkDraftError {
        NoDraft, UnknownTier, UnknownGroup, UnknownEntry,
        DuplicateScenarioName, PositionOutOfRange, MixedContent
    };

    struct BenchmarkDraftResult {
        std::optional<BenchmarkDraftError> error;
        std::optional<domain::TierId> createdTier;
        std::optional<domain::GroupId> createdGroup;
        std::optional<domain::ScenarioEntryId> createdEntry;
        [[nodiscard]] bool ok() const { return !error.has_value(); }
    };

    // monostate targets the fixed Uncategorized collection; a GroupId targets a
    // category or subcategory.
    using DraftGroupTarget = std::variant<std::monostate, domain::GroupId>;

    struct PlaylistImportResult {
        std::optional<PlaylistImportFailure> failure;
        std::vector<int> skippedDuplicateIndices;  // populated on success
        [[nodiscard]] bool ok() const { return !failure.has_value(); }
    };

    enum class BenchmarkSaveError { NoDraft, EmptyName, Conflict, WriteFailed };

    struct BenchmarkSaveResult {
        std::optional<BenchmarkSaveError> error;
        [[nodiscard]] bool ok() const { return !error.has_value(); }
    };

    enum class BenchmarkDeleteError { NotFound, Conflict, WriteFailed };

    struct BenchmarkDeleteOutcome {
        std::optional<BenchmarkDeleteError> error;
        [[nodiscard]] bool ok() const { return !error.has_value(); }
    };

    class IBenchmarkLibraryService {
    public:
        virtual ~IBenchmarkLibraryService() = default;
        [[nodiscard]] virtual std::optional<BenchmarkLibrarySnapshot> snapshot() const = 0;
        [[nodiscard]] virtual uint64_t revision() const = 0;
        [[nodiscard]] virtual bool lastRefreshFailed() const = 0;
        virtual void refresh() = 0;
        virtual void onChanged(std::function<void()> callback) = 0;
        [[nodiscard]] virtual std::string managedDirectoryPath() const = 0;
        [[nodiscard]] virtual std::vector<domain::ScenarioId> scenarioCatalogue() const = 0;
        [[nodiscard]] virtual std::vector<domain::ScenarioResolution> resolutionsFor(
            const domain::BenchmarkId &id) const = 0;
        [[nodiscard]] virtual std::vector<domain::ScenarioResolution> draftResolutions() const = 0;
        [[nodiscard]] virtual bool lastResolutionWriteFailed() const = 0;

        // ---- Single active manager draft -----------------------------------------
        //
        // The service is the sole mutation authority for the draft: every edit goes
        // through a command here, and the presentation layer reads back via draft() /
        // draftValidation() / draftDirty(). Commands return BenchmarkDraftResult rather
        // than throwing so QML can surface the failure verbatim.
        virtual void beginNewDraft() = 0;
        [[nodiscard]] virtual bool openDraft(const domain::BenchmarkId &id) = 0;
        virtual void discardDraft() = 0;
        [[nodiscard]] virtual std::optional<domain::Benchmark> draft() const = 0;
        [[nodiscard]] virtual const domain::CompletenessResult &draftValidation() const = 0;
        [[nodiscard]] virtual bool hasDraft() const = 0;
        [[nodiscard]] virtual bool draftDirty() const = 0;
        [[nodiscard]] virtual bool draftFromLibrary() const = 0; // opened from a saved file (retroactive-warning cue)
        virtual void onDraftChanged(std::function<void()> callback) = 0;

        virtual PlaylistImportResult importPlaylist(const std::string &path) = 0;

        virtual BenchmarkDraftResult renameBenchmark(const std::string &name) = 0;
        virtual BenchmarkDraftResult addTier(const std::string &name) = 0;
        virtual BenchmarkDraftResult renameTier(const domain::TierId &id, const std::string &name) = 0;
        virtual BenchmarkDraftResult setTierColor(const domain::TierId &id, domain::BenchmarkColor color) = 0;
        virtual BenchmarkDraftResult reorderTier(const domain::TierId &id, size_t position) = 0;
        // Also erases every entry's threshold for that tier, so the draft never holds a
        // dangling tier reference (the decoder classifies those as Invalid).
        virtual BenchmarkDraftResult removeTier(const domain::TierId &id) = 0;
        virtual BenchmarkDraftResult addUnplayedScenario(const std::string &name) = 0;
        virtual BenchmarkDraftResult addKnownScenario(const std::string &name, const std::string &hash) = 0;
        virtual BenchmarkDraftResult renameScenario(const domain::ScenarioEntryId &id, const std::string &name) = 0;
        virtual BenchmarkDraftResult setScenarioHash(const domain::ScenarioEntryId &id,
                                                     const std::optional<std::string> &hash) = 0;
        virtual BenchmarkDraftResult removeScenario(const domain::ScenarioEntryId &id) = 0;
        virtual BenchmarkDraftResult setThreshold(const domain::ScenarioEntryId &entryId,
                                                  const domain::TierId &tierId, double score) = 0;
        virtual BenchmarkDraftResult clearThreshold(const domain::ScenarioEntryId &entryId,
                                                    const domain::TierId &tierId) = 0;
        virtual BenchmarkDraftResult addCategory(const std::string &name) = 0;
        virtual BenchmarkDraftResult renameGroup(const domain::GroupId &id, const std::string &name) = 0;
        virtual BenchmarkDraftResult setGroupColor(const domain::GroupId &id, domain::BenchmarkColor color) = 0;
        virtual BenchmarkDraftResult reorderCategory(const domain::GroupId &id, size_t position) = 0;
        virtual BenchmarkDraftResult removeCategory(const domain::GroupId &id) = 0;
        // If the category holds direct scenarios, they all move into a newly created
        // subcategory first, so the category never ends up in a mixed shape.
        virtual BenchmarkDraftResult addSubcategory(const domain::GroupId &categoryId, const std::string &name) = 0;
        virtual BenchmarkDraftResult moveScenario(const domain::ScenarioEntryId &id, const DraftGroupTarget &to) = 0;

        virtual BenchmarkSaveResult saveDraft() = 0;
        // Only loaded benchmarks are addressable; problem entries have no stable handle
        // (an unparseable file exposes no id and no trustworthy digest).
        virtual BenchmarkDeleteOutcome deleteBenchmark(const domain::BenchmarkId &id) = 0;
    };
}
