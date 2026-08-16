//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H

#include <utility>

#include "contracts/i_graph_use_case.h"
#include "i_session_controller.h"
#include "perf_column_builder.h"

namespace ksv::application {
    class GraphUseCase: public IGraphUseCase {
    public:
        explicit GraphUseCase(std::shared_ptr<ISessionController> session_controller): m_session_controller(std::move(session_controller)) {}

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

    private:
        std::shared_ptr<ISessionController> m_session_controller;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H
