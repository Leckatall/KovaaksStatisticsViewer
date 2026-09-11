#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "contracts/benchmark_editor_seed.h"
#include "contracts/benchmark_manager_state.h"
#include "contracts/i_benchmark_manager_use_case.h"
#include "contracts/i_benchmark_resolution_use_case.h"
#include "data/interfaces/i_benchmarks_service.h"
#include "data/interfaces/i_playlist_reader.h"

namespace ksv::application {
    class BenchmarkManagerUseCase final : public IBenchmarkManagerUseCase {
    public:
        BenchmarkManagerUseCase(std::shared_ptr<data::IBenchmarksService> benchmarks,
                                std::shared_ptr<IBenchmarkResolutionUseCase> resolution,
                                std::shared_ptr<IPlaylistReader> playlist,
                                std::function<std::string()> idFactory);

        [[nodiscard]] const BenchmarkManagerState &state() const override { return m_state; }
        void onChanged(std::function<void()> callback) override {
            m_callbacks.push_back(std::move(callback));
        }

        void refresh() override { m_benchmarks->refresh(); }
        [[nodiscard]] std::string managedDirectoryPath() const override {
            return m_benchmarks->managedDirectoryPath();
        }

        [[nodiscard]] BenchmarkEditorSeed beginNewBenchmark() override;
        [[nodiscard]] std::optional<BenchmarkEditorSeed> openBenchmark(const domain::BenchmarkId &id) override;
        void closeEditor() override;
        [[nodiscard]] PlaylistSeedImport importPlaylistSeed(const std::string &path) override;

        [[nodiscard]] data::BenchmarkSaveOutcome save(
            const domain::Benchmark &benchmark,
            const std::optional<data::BenchmarkEditToken> &token) override;
        [[nodiscard]] std::vector<domain::ScenarioResolution> resolve(
            const domain::Benchmark &benchmark) const override;
        [[nodiscard]] data::BenchmarkRemoveOutcome deleteBenchmark(
            const data::BenchmarkEditToken &token) override;

    private:
        [[nodiscard]] BenchmarkManagerState build() const;
        void rebuildAndNotify();
        [[nodiscard]] std::string newId() const { return m_idFactory(); }

        std::shared_ptr<data::IBenchmarksService> m_benchmarks;
        std::shared_ptr<IBenchmarkResolutionUseCase> m_resolution;
        std::shared_ptr<IPlaylistReader> m_playlist;
        std::function<std::string()> m_idFactory;
        BenchmarkManagerState m_state;
        std::vector<std::function<void()> > m_callbacks;
    };
}
