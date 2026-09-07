#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "contracts/i_benchmark_library_service.h"
#include "data/interfaces/i_benchmark_repository.h"
#include "data/interfaces/i_playlist_reader.h"

namespace ksv::application {
    class BenchmarkLibraryService final : public IBenchmarkLibraryService {
    public:
        BenchmarkLibraryService(std::shared_ptr<IBenchmarkRepository> repository,
                                std::shared_ptr<IPlaylistReader> playlistReader,
                                std::function<std::string()> idFactory);

        [[nodiscard]] std::optional<BenchmarkLibrarySnapshot> snapshot() const override { return m_snapshot; }
        [[nodiscard]] uint64_t revision() const override { return m_revision; }
        [[nodiscard]] bool lastRefreshFailed() const override { return m_lastRefreshFailed; }
        void refresh() override;
        void onChanged(std::function<void()> callback) override { m_callbacks.push_back(std::move(callback)); }
        [[nodiscard]] std::string managedDirectoryPath() const override {
            return m_repository->managedDirectoryPath();
        }

        void beginNewDraft() override;
        [[nodiscard]] bool openDraft(const domain::BenchmarkId &id) override;
        void discardDraft() override;
        [[nodiscard]] std::optional<domain::Benchmark> draft() const override {
            return m_draft ? std::optional(m_draft->benchmark) : std::nullopt;
        }
        [[nodiscard]] const domain::CompletenessResult &draftValidation() const override {
            static const domain::CompletenessResult empty{};
            return m_draft ? m_draft->validation : empty;
        }
        [[nodiscard]] bool hasDraft() const override { return m_draft.has_value(); }
        [[nodiscard]] bool draftDirty() const override { return m_draft && m_draft->dirty; }
        [[nodiscard]] bool draftFromLibrary() const override {
            return m_draft && m_draft->baselineFilename.has_value();
        }
        void onDraftChanged(std::function<void()> callback) override {
            m_draftCallbacks.push_back(std::move(callback));
        }

        PlaylistImportResult importPlaylist(const std::string &path) override;

        BenchmarkDraftResult renameBenchmark(const std::string &name) override;
        BenchmarkDraftResult addTier(const std::string &name) override;
        BenchmarkDraftResult renameTier(const domain::TierId &id, const std::string &name) override;
        BenchmarkDraftResult setTierColor(const domain::TierId &id, domain::BenchmarkColor color) override;
        BenchmarkDraftResult reorderTier(const domain::TierId &id, size_t position) override;
        BenchmarkDraftResult removeTier(const domain::TierId &id) override;
        BenchmarkDraftResult addUnplayedScenario(const std::string &name) override;
        BenchmarkDraftResult addKnownScenario(const std::string &name, const std::string &hash) override;
        BenchmarkDraftResult renameScenario(const domain::ScenarioEntryId &id, const std::string &name) override;
        BenchmarkDraftResult removeScenario(const domain::ScenarioEntryId &id) override;
        BenchmarkDraftResult setThreshold(const domain::ScenarioEntryId &entryId,
                                          const domain::TierId &tierId, double score) override;
        BenchmarkDraftResult clearThreshold(const domain::ScenarioEntryId &entryId,
                                            const domain::TierId &tierId) override;
        BenchmarkDraftResult addCategory(const std::string &name) override;
        BenchmarkDraftResult renameGroup(const domain::GroupId &id, const std::string &name) override;
        BenchmarkDraftResult setGroupColor(const domain::GroupId &id, domain::BenchmarkColor color) override;
        BenchmarkDraftResult reorderCategory(const domain::GroupId &id, size_t position) override;
        BenchmarkDraftResult removeCategory(const domain::GroupId &id) override;
        BenchmarkDraftResult addSubcategory(const domain::GroupId &categoryId, const std::string &name) override;
        BenchmarkDraftResult moveScenario(const domain::ScenarioEntryId &id, const DraftGroupTarget &to) override;

        BenchmarkSaveResult saveDraft() override;
        BenchmarkDeleteOutcome deleteBenchmark(const domain::BenchmarkId &id) override;

    private:
        // The in-progress working copy of one benchmark. `baseline` is the last saved (or
        // opened) content: dirty means benchmark has drifted from it, and baselineDigest is
        // the write precondition for the next save.
        struct Draft {
            domain::Benchmark benchmark;
            std::optional<std::string> baselineFilename;   // nullopt for a never-saved new draft
            std::optional<std::string> baselineDigest;     // expectedDigest for the next write
            domain::Benchmark baseline;
            domain::CompletenessResult validation;
            bool dirty = false;
        };

        std::string newId() const { return m_idFactory(); }
        void revalidateDraft();
        void notifyDraftChanged();
        void publish();
        void upsertSnapshotEntry(const std::string &filename, const std::string &digest,
                                 LoadedBenchmark content);

        std::shared_ptr<IBenchmarkRepository> m_repository;
        std::shared_ptr<IPlaylistReader> m_playlistReader;
        std::function<std::string()> m_idFactory;
        std::optional<BenchmarkLibrarySnapshot> m_snapshot;
        uint64_t m_revision = 0;
        bool m_lastRefreshFailed = false;
        std::vector<std::function<void()>> m_callbacks;
        std::optional<Draft> m_draft;
        std::vector<std::function<void()>> m_draftCallbacks;
    };
}
