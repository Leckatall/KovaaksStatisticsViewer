//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H

#include <utility>

#include "contracts/i_graph_use_case.h"
#include "i_session_controller.h"
#include "perf_column_builder.h"
#include "bucketed_run.h"
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

        GraphSeries get_series() override {
            return PerfColumnBuilder::build(m_session_controller->getCurrentPerf());
        }

        std::string get_run_label() override {
            return m_session_controller->getCurrentPerf().run_id.toString();
        }

        void onCurrentPerfChanged(std::function<void()> callback) override {
            QObject::connect(m_session_controller.get(), &ISessionController::currentPerfChanged,
                              m_session_controller.get(), std::move(callback));
        }

        [[nodiscard]] ResolvedGraph get_resolved_graph() override {
            ResolvedGraph result;
            if (!m_session_controller) return result;
            const auto run = m_session_controller->getCurrentPerf();
            const auto bucketed = bucketRun(run);
            result.times = bucketed.times;
            const auto configs = m_store ? m_store->getAll() : std::vector<SeriesConfig>{};
            for (const auto &config : configs) {
                std::optional<std::vector<double>> values;
                if (const auto *base = std::get_if<BaseSeriesConfig>(&config)) {
                    values = bucketed.valuesFor(base->metric);
                } else if (m_average) {
                    values = m_average->evaluate(run, std::get<ComputedSeriesConfig>(config).expression);
                }
                result.series.push_back({config, std::move(values)});
            }
            return result;
        }

        MutationResult setSeriesEnabled(const SeriesRecordReference reference, const bool enabled) override {
            if (!m_store) return publishLegacy();
            if (const auto metric = std::get_if<PrimitiveMetric>(&reference)) {
                const auto configs = m_store->getAll();
                for (const auto &config : configs) if (const auto *base = std::get_if<BaseSeriesConfig>(&config); base && base->metric == *metric)
                    return updateBasePresentation({*metric, enabled, base->presentation.lineStyle});
            }
            if (const auto id = std::get_if<ComputedSeriesId>(&reference)) {
                const auto configs = m_store->getAll();
                for (const auto &config : configs) if (const auto *computed = std::get_if<ComputedSeriesConfig>(&config); computed && computed->id == *id)
                    return updateComputed({*id, {computed->presentation.name, computed->presentation.lineStyle, enabled}, computed->expression});
            }
            return {};
        }
        MutationResult updateBasePresentation(const UpdateBaseSeriesRequest &request) override { return m_store ? m_store->updateBase(request) : publishLegacy(); }
        MutationResult createComputed(const CreateComputedSeriesRequest &request) override { return m_store ? m_store->createComputed(request) : publishLegacy(); }
        MutationResult updateComputed(const UpdateComputedSeriesRequest &request) override { return m_store ? m_store->updateComputed(request) : publishLegacy(); }
        MutationResult removeComputed(const ComputedSeriesId id) override { return m_store ? m_store->removeComputed(id) : publishLegacy(); }
        MutationResult moveSeries(const SeriesRecordReference reference, const uint32_t position) override { return m_store ? m_store->reorder(reference, position) : publishLegacy(); }
        void onSeriesConfigChanged(std::function<void()> callback) override { m_config_callbacks.push_back(std::move(callback)); }

    private:
        std::shared_ptr<ISessionController> m_session_controller;
        std::shared_ptr<ISeriesConfigStore> m_store;
        std::shared_ptr<IAverageLineUseCase> m_average;
        std::vector<std::function<void()>> m_config_callbacks;

        MutationResult publishLegacy() {
            for (const auto &callback : m_config_callbacks) callback();
            return {};
        }
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H
