#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_MANAGER_USE_CASE_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_MANAGER_USE_CASE_H

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "contracts/benchmark_manager_state.h"
#include "contracts/i_benchmark_manager_use_case.h"

namespace ksv::tests_support {
    // In-memory IBenchmarkManagerUseCase for manager presentation tests. `state` is the single
    // coherent revision the view model reads after each `notify()`; callers mutate it directly and
    // fire `notify()` to model one service publication. Every command appends its name to
    // `commandLog`, records the arguments the tests assert on, and returns the entry registered in
    // `scriptedDraftResults` (or a default ok result).
    class FakeBenchmarkManagerUseCase final : public application::IBenchmarkManagerUseCase {
    public:
        application::BenchmarkManagerState stateValue{};
        std::vector<std::function<void()>> changedCallbacks;

        std::vector<std::string> commandLog;
        std::map<std::string, application::BenchmarkDraftResult> scriptedDraftResults;

        std::vector<std::string> renameBenchmarkArgs;
        std::vector<std::string> addTierNames;
        std::vector<std::string> addUnplayedScenarioNames;
        std::vector<std::pair<std::string, std::string>> addKnownScenarioArgs;
        std::vector<std::pair<std::string, std::optional<std::string>>> setScenarioHashArgs;
        std::vector<std::tuple<std::string, std::string, double>> setThresholdArgs;
        std::vector<std::pair<std::string, std::string>> clearThresholdArgs;
        std::vector<std::string> addCategoryNames;
        std::vector<std::pair<std::string, std::string>> addSubcategoryArgs;
        std::vector<std::pair<std::string, application::DraftGroupTarget>> moveScenarioArgs;
        std::vector<std::string> openDraftArgs;
        std::vector<std::string> deleteArgs;
        std::vector<std::string> importPaths;

        application::PlaylistImportResult importResult{};
        application::BenchmarkSaveResult saveResult{};
        application::BenchmarkDeleteOutcome deleteOutcome{};
        bool openDraftResult = true;
        std::string managedDirectory = "C:/Benchmarks";

        void notify() {
            for (const auto &callback: changedCallbacks) callback();
        }

        [[nodiscard]] const application::BenchmarkManagerState &state() const override { return stateValue; }
        void onChanged(std::function<void()> callback) override {
            changedCallbacks.push_back(std::move(callback));
        }

        void refresh() override { commandLog.emplace_back("refresh"); }
        [[nodiscard]] std::string managedDirectoryPath() const override { return managedDirectory; }

        void beginNewDraft() override { commandLog.emplace_back("beginNewDraft"); }
        [[nodiscard]] bool openDraft(const domain::BenchmarkId &id) override {
            commandLog.emplace_back("openDraft");
            openDraftArgs.push_back(id.value);
            return openDraftResult;
        }
        void discardDraft() override { commandLog.emplace_back("discardDraft"); }

        application::PlaylistImportResult importPlaylist(const std::string &path) override {
            commandLog.emplace_back("importPlaylist");
            importPaths.push_back(path);
            return importResult;
        }

        application::BenchmarkDraftResult renameBenchmark(const std::string &name) override {
            renameBenchmarkArgs.push_back(name);
            return draftResult("renameBenchmark");
        }
        application::BenchmarkDraftResult addTier(const std::string &name) override {
            addTierNames.push_back(name);
            return draftResult("addTier");
        }
        application::BenchmarkDraftResult renameTier(const domain::TierId &id, const std::string &name) override {
            (void) id;
            (void) name;
            return draftResult("renameTier");
        }
        application::BenchmarkDraftResult setTierColor(const domain::TierId &id, domain::BenchmarkColor color) override {
            (void) id;
            (void) color;
            return draftResult("setTierColor");
        }
        application::BenchmarkDraftResult reorderTier(const domain::TierId &id, std::size_t position) override {
            (void) id;
            (void) position;
            return draftResult("reorderTier");
        }
        application::BenchmarkDraftResult removeTier(const domain::TierId &id) override {
            (void) id;
            return draftResult("removeTier");
        }
        application::BenchmarkDraftResult addUnplayedScenario(const std::string &name) override {
            addUnplayedScenarioNames.push_back(name);
            return draftResult("addUnplayedScenario");
        }
        application::BenchmarkDraftResult addKnownScenario(const std::string &name, const std::string &hash) override {
            addKnownScenarioArgs.emplace_back(name, hash);
            return draftResult("addKnownScenario");
        }
        application::BenchmarkDraftResult renameScenario(const domain::ScenarioEntryId &id,
                                                        const std::string &name) override {
            (void) id;
            (void) name;
            return draftResult("renameScenario");
        }
        application::BenchmarkDraftResult setScenarioHash(const domain::ScenarioEntryId &id,
                                                         const std::optional<std::string> &hash) override {
            setScenarioHashArgs.emplace_back(id.value, hash);
            return draftResult("setScenarioHash");
        }
        application::BenchmarkDraftResult removeScenario(const domain::ScenarioEntryId &id) override {
            (void) id;
            return draftResult("removeScenario");
        }
        application::BenchmarkDraftResult setThreshold(const domain::ScenarioEntryId &entryId,
                                                      const domain::TierId &tierId, double score) override {
            setThresholdArgs.emplace_back(entryId.value, tierId.value, score);
            return draftResult("setThreshold");
        }
        application::BenchmarkDraftResult clearThreshold(const domain::ScenarioEntryId &entryId,
                                                        const domain::TierId &tierId) override {
            clearThresholdArgs.emplace_back(entryId.value, tierId.value);
            return draftResult("clearThreshold");
        }
        application::BenchmarkDraftResult addCategory(const std::string &name) override {
            addCategoryNames.push_back(name);
            return draftResult("addCategory");
        }
        application::BenchmarkDraftResult renameGroup(const domain::GroupId &id, const std::string &name) override {
            (void) id;
            (void) name;
            return draftResult("renameGroup");
        }
        application::BenchmarkDraftResult setGroupColor(const domain::GroupId &id, domain::BenchmarkColor color) override {
            (void) id;
            (void) color;
            return draftResult("setGroupColor");
        }
        application::BenchmarkDraftResult reorderCategory(const domain::GroupId &id, std::size_t position) override {
            (void) id;
            (void) position;
            return draftResult("reorderCategory");
        }
        application::BenchmarkDraftResult removeCategory(const domain::GroupId &id) override {
            (void) id;
            return draftResult("removeCategory");
        }
        application::BenchmarkDraftResult addSubcategory(const domain::GroupId &categoryId,
                                                        const std::string &name) override {
            addSubcategoryArgs.emplace_back(categoryId.value, name);
            return draftResult("addSubcategory");
        }
        application::BenchmarkDraftResult moveScenario(const domain::ScenarioEntryId &id,
                                                      const application::DraftGroupTarget &to) override {
            moveScenarioArgs.emplace_back(id.value, to);
            return draftResult("moveScenario");
        }

        application::BenchmarkSaveResult saveDraft() override {
            commandLog.emplace_back("saveDraft");
            return saveResult;
        }
        application::BenchmarkDeleteOutcome deleteBenchmark(const domain::BenchmarkId &id) override {
            deleteArgs.push_back(id.value);
            return deleteOutcome;
        }

    private:
        application::BenchmarkDraftResult draftResult(const std::string &command) {
            commandLog.push_back(command);
            const auto found = scriptedDraftResults.find(command);
            return found == scriptedDraftResults.end() ? application::BenchmarkDraftResult{} : found->second;
        }
    };
}

#endif // KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_MANAGER_USE_CASE_H
