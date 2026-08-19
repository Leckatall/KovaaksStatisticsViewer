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
            const auto run = m_session_controller->getCurrentPerf();
            const auto bucketed = bucketRun(run);
            result.times = bucketed.times;
            for (const SeriesConfig &config : m_store->getAll()) {
                std::optional<std::vector<double>> values;
                values = m_average->evaluate(run, config.expression);
                result.series.push_back({config, std::move(values)});
            }
            return result;
        }

        MutationResult setSeriesEnabled(const SeriesId series_id, const bool enabled) override {
            UpdatedSeriesPresentation presentation{.enabled = enabled};
            return m_store->updateSeries({.id = series_id, .presentation = presentation});
        }
        MutationResult updateBasePresentation(const UpdateBaseSeriesRequest &request) override { return m_store->updateBase(request); }
        MutationResult createComputed(const CreateComputedSeriesRequest &request) override { return m_store->createComputed(request);}
        MutationResult updateComputed(const UpdateComputedSeriesRequest &request) override { return m_store->updateComputed(request);}
        MutationResult removeComputed(const SeriesId id) override { return m_store->removeComputed(id);}
        MutationResult moveSeries(const SeriesId id, const uint32_t position) override { return m_store->reorder(id, position); }
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
