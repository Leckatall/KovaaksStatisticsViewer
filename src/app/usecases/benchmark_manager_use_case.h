#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "contracts/benchmark_manager_state.h"
#include "contracts/i_benchmark_library_service.h"
#include "contracts/i_benchmark_manager_use_case.h"

namespace ksv::application {
    class BenchmarkManagerUseCase final : public IBenchmarkManagerUseCase {
    public:
        explicit BenchmarkManagerUseCase(std::shared_ptr<IBenchmarkLibraryService> library);

        [[nodiscard]] const BenchmarkManagerState &state() const override { return m_state; }
        void onChanged(std::function<void()> callback) override {
            m_callbacks.push_back(std::move(callback));
        }

        void refresh() override { m_library->refresh(); }
        [[nodiscard]] std::string managedDirectoryPath() const override {
            return m_library->managedDirectoryPath();
        }

        void beginNewDraft() override { m_library->beginNewDraft(); }
        [[nodiscard]] bool openDraft(const domain::BenchmarkId &id) override {
            return m_library->openDraft(id);
        }
        void discardDraft() override { m_library->discardDraft(); }

        PlaylistImportResult importPlaylist(const std::string &path) override {
            return m_library->importPlaylist(path);
        }

        BenchmarkDraftResult renameBenchmark(const std::string &name) override {
            return m_library->renameBenchmark(name);
        }
        BenchmarkDraftResult addTier(const std::string &name) override {
            return m_library->addTier(name);
        }
        BenchmarkDraftResult renameTier(const domain::TierId &id, const std::string &name) override {
            return m_library->renameTier(id, name);
        }
        BenchmarkDraftResult setTierColor(const domain::TierId &id, domain::BenchmarkColor color) override {
            return m_library->setTierColor(id, color);
        }
        BenchmarkDraftResult reorderTier(const domain::TierId &id, std::size_t position) override {
            return m_library->reorderTier(id, position);
        }
        BenchmarkDraftResult removeTier(const domain::TierId &id) override {
            return m_library->removeTier(id);
        }
        BenchmarkDraftResult addUnplayedScenario(const std::string &name) override {
            return m_library->addUnplayedScenario(name);
        }
        BenchmarkDraftResult addKnownScenario(const std::string &name, const std::string &hash) override {
            return m_library->addKnownScenario(name, hash);
        }
        BenchmarkDraftResult renameScenario(const domain::ScenarioEntryId &id, const std::string &name) override {
            return m_library->renameScenario(id, name);
        }
        BenchmarkDraftResult setScenarioHash(const domain::ScenarioEntryId &id,
                                             const std::optional<std::string> &hash) override {
            return m_library->setScenarioHash(id, hash);
        }
        BenchmarkDraftResult removeScenario(const domain::ScenarioEntryId &id) override {
            return m_library->removeScenario(id);
        }
        BenchmarkDraftResult setThreshold(const domain::ScenarioEntryId &entryId,
                                          const domain::TierId &tierId, double score) override {
            return m_library->setThreshold(entryId, tierId, score);
        }
        BenchmarkDraftResult clearThreshold(const domain::ScenarioEntryId &entryId,
                                            const domain::TierId &tierId) override {
            return m_library->clearThreshold(entryId, tierId);
        }
        BenchmarkDraftResult addCategory(const std::string &name) override {
            return m_library->addCategory(name);
        }
        BenchmarkDraftResult renameGroup(const domain::GroupId &id, const std::string &name) override {
            return m_library->renameGroup(id, name);
        }
        BenchmarkDraftResult setGroupColor(const domain::GroupId &id, domain::BenchmarkColor color) override {
            return m_library->setGroupColor(id, color);
        }
        BenchmarkDraftResult reorderCategory(const domain::GroupId &id, std::size_t position) override {
            return m_library->reorderCategory(id, position);
        }
        BenchmarkDraftResult removeCategory(const domain::GroupId &id) override {
            return m_library->removeCategory(id);
        }
        BenchmarkDraftResult addSubcategory(const domain::GroupId &categoryId, const std::string &name) override {
            return m_library->addSubcategory(categoryId, name);
        }
        BenchmarkDraftResult moveScenario(const domain::ScenarioEntryId &id, const DraftGroupTarget &to) override {
            return m_library->moveScenario(id, to);
        }

        BenchmarkSaveResult saveDraft() override { return m_library->saveDraft(); }
        BenchmarkDeleteOutcome deleteBenchmark(const domain::BenchmarkId &id) override {
            return m_library->deleteBenchmark(id);
        }

    private:
        [[nodiscard]] BenchmarkManagerState build() const;
        void rebuildAndNotify();

        std::shared_ptr<IBenchmarkLibraryService> m_library;
        BenchmarkManagerState m_state;
        std::vector<std::function<void()> > m_callbacks;
    };
}
