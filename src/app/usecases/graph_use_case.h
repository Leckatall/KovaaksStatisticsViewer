//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H

#include <utility>

#include "i_graph_use_case.h"
#include "i_session_controller.h"

namespace ksv::application {
    class GraphUseCase: public IGraphUseCase {
    public:
        explicit GraphUseCase(std::shared_ptr<ISessionController> session_controller): m_session_controller(std::move(session_controller)) {}

        void load_perf(const std::string_view filename) override {
            std::cout << "Loading perf file: " << filename << std::endl;
            m_session_controller->setCurrentPerf(filename.data());
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
