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

        std::vector<float> get_times() override {
            const domain::ScenarioPerf perf = m_session_controller->getCurrentPerf();
            std::vector<float> times;
            for (const auto &point: perf.data) {
                times.push_back(point.time);
            }
            return times;
        }

        std::vector<float> get_scores() override {
            const domain::ScenarioPerf perf = m_session_controller->getCurrentPerf();
            std::vector<float> scores;
            for (const auto &point: perf.data) {
                scores.push_back(point.score);
            }
            return scores;
        }

        std::vector<float> get_accuracies() override {
            const domain::ScenarioPerf perf = m_session_controller->getCurrentPerf();
            std::vector<float> accuracies;
            for (const auto &point: perf.data) {
                if (point.shots == 0) accuracies.push_back(0);
                else accuracies.push_back(static_cast<float>(point.hits) / static_cast<float>(point.shots));
            }
            return accuracies;
        }

        std::vector<int> get_shots() override {
            const domain::ScenarioPerf perf = m_session_controller->getCurrentPerf();
            std::vector<int> shots;
            for (const auto &point: perf.data) {
                shots.push_back(point.shots);
            }
            return shots;
        }

        std::vector<int> get_kills() override {
            const domain::ScenarioPerf perf = m_session_controller->getCurrentPerf();
            std::vector<int> kills;
            for (const auto &point: perf.data) {
                kills.push_back(point.kills);
            }
            return kills;
        }
        std::vector<float> get_dmg() override {
            const domain::ScenarioPerf perf = m_session_controller->getCurrentPerf();
            std::vector<float> dmg;
            for (const auto &point: perf.data) {
                dmg.push_back(point.dmg);
            }
            return dmg;
        }

    private:
        std::shared_ptr<ISessionController> m_session_controller;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H
