//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H

#include <algorithm>
#include <optional>
#include <utility>

#include "contracts/i_graph_use_case.h"
#include "i_session_controller.h"
#include "bucketed_run.h"
#include "../contracts/graph_info.h"
#include "../contracts/i_average_line_use_case.h"
#include "../../data/interfaces/i_series_config_store.h"

namespace ksv::application {
    class GraphUseCase: public IGraphUseCase {
    public:
        explicit GraphUseCase(std::shared_ptr<ISessionController> session_controller): m_session_controller(std::move(session_controller)) {}
        GraphUseCase(std::shared_ptr<ISessionController> session_controller,
                     std::shared_ptr<ISeriesConfigStore> store,
                     std::shared_ptr<IAverageLineUseCase> average)
            : m_session_controller(std::move(session_controller)), m_store(std::move(store)), m_average(std::move(average)) {
            if (m_store) m_store->onChanged([this] { for (const auto &callback : m_config_callbacks) callback(); });
        }

        void load_perf(const std::string_view filename) override {
            m_session_controller->setCurrentPerf(std::string(filename));
        }
        void load_latest_perf() override {
            m_session_controller->setCurrentPerfToLatest();
        }

        std::string get_run_label() override {
            return m_session_controller->getCurrentPerf().run_id.toString();
        }

        void onCurrentPerfChanged(std::function<void()> callback) override {
            QObject::connect(m_session_controller.get(), &ISessionController::currentPerfChanged,
                              m_session_controller.get(), std::move(callback));
        }

        [[nodiscard]] std::vector<SeriesConfig> getSeriesConfigs() override {
            std::vector<SeriesConfig> result;
            for (const auto &config : m_store->getAll())
                if (config.presentation.enabled) result.push_back(config);
            return result;
        }

        [[nodiscard]] std::optional<SeriesPoints> getSeriesValues(const SeriesId id) override {
            for (const auto &config : m_store->getAll()) {
                if (config.id != id) continue;
                const auto ys = m_average->evaluate(m_session_controller->getCurrentPerf(), config.expression);
                if (!ys) return std::nullopt;
                const auto &buckets = referenceBuckets();
                const auto count = std::min(buckets.times.size(), ys->size());
                SeriesPoints points;
                points.reserve(count);
                for (size_t i = 0; i < count; ++i)
                    points.emplace_back(static_cast<double>(buckets.times[i]), (*ys)[i]);
                return points;
            }
            return std::nullopt;
        }

        [[nodiscard]] std::vector<AxisConfig> getAxes() override { return m_store->getAllAxes(); }

        [[nodiscard]] double getRunDuration() override {
            const auto &buckets = referenceBuckets();
            return buckets.times.empty() ? 0.0 : static_cast<double>(buckets.times.back());
        }

        void onSeriesConfigChanged(std::function<void()> callback) override { m_config_callbacks.push_back(std::move(callback)); }

    private:
        // Single-entry cache of the reference run's buckets, shared by getSeriesValues()'s x-fold
        // and getRunDuration(). Keyed by run id; a run is immutable under its id, so an id match is
        // a valid hit. AverageLineUseCase::evaluate() buckets the run again internally by design —
        // this cache only removes the separate bucketing for point x-values and the run duration.
        const BucketedRun &referenceBuckets() {
            const auto perf = m_session_controller->getCurrentPerf();
            if (!m_cachedRunId || *m_cachedRunId != perf.run_id) {
                m_cachedBuckets = bucketRun(perf);
                m_cachedRunId = perf.run_id;
            }
            return m_cachedBuckets;
        }

        std::shared_ptr<ISessionController> m_session_controller;
        std::shared_ptr<ISeriesConfigStore> m_store;
        std::shared_ptr<IAverageLineUseCase> m_average;
        std::vector<std::function<void()>> m_config_callbacks;
        std::optional<domain::ScenarioRunId> m_cachedRunId;
        BucketedRun m_cachedBuckets;

    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H
