#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_MANAGER_USE_CASE_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_MANAGER_USE_CASE_H

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "contracts/benchmark_editor_seed.h"
#include "contracts/benchmark_manager_state.h"
#include "contracts/i_benchmark_manager_use_case.h"

namespace ksv::tests_support {
    // In-memory IBenchmarkManagerUseCase for manager presentation tests. `stateValue` is the single
    // coherent revision the view model reads after each `notify()`; callers mutate it directly.
    // Editor-seed returns and save/delete/resolve outcomes are scriptable, and every call is
    // recorded for the tests to assert on.
    class FakeBenchmarkManagerUseCase final : public application::IBenchmarkManagerUseCase {
    public:
        application::BenchmarkManagerState stateValue{};
        std::vector<std::function<void()>> changedCallbacks;
        std::vector<std::string> commandLog;

        application::BenchmarkEditorSeed nextNewSeed{};
        // std::nullopt models an unknown / problem entry: openBenchmark then returns std::nullopt.
        std::optional<application::BenchmarkEditorSeed> nextOpenSeed{};
        application::PlaylistSeedImport nextImport{};

        data::BenchmarkSaveOutcome nextSaveOutcome{};
        data::BenchmarkRemoveOutcome nextRemoveOutcome{};
        mutable std::vector<domain::ScenarioResolution> nextResolveResult;

        std::vector<std::string> openArgs;
        std::vector<std::string> importPaths;
        std::vector<std::pair<domain::Benchmark, std::optional<data::BenchmarkEditToken>>> saveCalls;
        std::vector<data::BenchmarkEditToken> deleteTokens;
        mutable std::vector<domain::Benchmark> resolveCalls;
        int beginNewCount = 0;
        int closeEditorCount = 0;

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

        [[nodiscard]] application::BenchmarkEditorSeed beginNewBenchmark() override {
            commandLog.emplace_back("beginNewBenchmark");
            ++beginNewCount;
            return nextNewSeed;
        }
        [[nodiscard]] std::optional<application::BenchmarkEditorSeed> openBenchmark(
            const domain::BenchmarkId &id) override {
            commandLog.emplace_back("openBenchmark");
            openArgs.push_back(id.value);
            return nextOpenSeed;
        }
        void closeEditor() override {
            commandLog.emplace_back("closeEditor");
            ++closeEditorCount;
        }
        [[nodiscard]] application::PlaylistSeedImport importPlaylistSeed(const std::string &path) override {
            commandLog.emplace_back("importPlaylistSeed");
            importPaths.push_back(path);
            return nextImport;
        }

        [[nodiscard]] data::BenchmarkSaveOutcome save(
            const domain::Benchmark &benchmark,
            const std::optional<data::BenchmarkEditToken> &token) override {
            commandLog.emplace_back("save");
            saveCalls.emplace_back(benchmark, token);
            return nextSaveOutcome;
        }
        [[nodiscard]] std::vector<domain::ScenarioResolution> resolve(
            const domain::Benchmark &benchmark) const override {
            resolveCalls.push_back(benchmark);
            return nextResolveResult;
        }
        [[nodiscard]] data::BenchmarkRemoveOutcome deleteBenchmark(
            const data::BenchmarkEditToken &token) override {
            commandLog.emplace_back("deleteBenchmark");
            deleteTokens.push_back(token);
            return nextRemoveOutcome;
        }
    };
}

#endif // KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_MANAGER_USE_CASE_H
