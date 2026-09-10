#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

#include "contracts/i_benchmark_library_service.h"

namespace ksv::application {
    struct BenchmarkManagerState;

    // Sole application boundary for the Benchmark Manager. The read side exposes one coherent state
    // and one notification channel adapted from the service's separate library and draft
    // publications. The command side mirrors IBenchmarkLibraryService one-for-one and forwards each
    // call unchanged; BenchmarkLibraryService remains the only persistence, draft, validation, and
    // transaction authority.
    class IBenchmarkManagerUseCase {
    public:
        virtual ~IBenchmarkManagerUseCase() = default;

        // Valid until the next service callback; never retained by callers.
        [[nodiscard]] virtual const BenchmarkManagerState &state() const = 0;
        virtual void onChanged(std::function<void()> callback) = 0;

        virtual void refresh() = 0;
        [[nodiscard]] virtual std::string managedDirectoryPath() const = 0;

        virtual void beginNewDraft() = 0;
        [[nodiscard]] virtual bool openDraft(const domain::BenchmarkId &id) = 0;
        virtual void discardDraft() = 0;

        virtual PlaylistImportResult importPlaylist(const std::string &path) = 0;

        virtual BenchmarkDraftResult renameBenchmark(const std::string &name) = 0;
        virtual BenchmarkDraftResult addTier(const std::string &name) = 0;
        virtual BenchmarkDraftResult renameTier(const domain::TierId &id, const std::string &name) = 0;
        virtual BenchmarkDraftResult setTierColor(const domain::TierId &id, domain::BenchmarkColor color) = 0;
        virtual BenchmarkDraftResult reorderTier(const domain::TierId &id, std::size_t position) = 0;
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
        virtual BenchmarkDraftResult reorderCategory(const domain::GroupId &id, std::size_t position) = 0;
        virtual BenchmarkDraftResult removeCategory(const domain::GroupId &id) = 0;
        virtual BenchmarkDraftResult addSubcategory(const domain::GroupId &categoryId, const std::string &name) = 0;
        virtual BenchmarkDraftResult moveScenario(const domain::ScenarioEntryId &id, const DraftGroupTarget &to) = 0;

        virtual BenchmarkSaveResult saveDraft() = 0;
        virtual BenchmarkDeleteOutcome deleteBenchmark(const domain::BenchmarkId &id) = 0;
    };
}
