//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H

#include "i_graph_use_case.h"
#include "interfaces/i_proto_decoder.h"

namespace ksv::application {
    class GraphUseCase: public IGraphUseCase {
    public:
        explicit GraphUseCase(IProtoDecoder& decoder): m_decoder(decoder) {}

        std::vector<float> get_times(const std::string_view filename) override {
            const domain::ScenarioPerf perf = get_data(filename);
            std::vector<float> times;
            for (const auto &point: perf.data) {
                times.push_back(point.time);
            }
            return times;
        }

        std::vector<float> get_scores(const std::string_view filename) override {
            const domain::ScenarioPerf perf = get_data(filename);
            std::vector<float> scores;
            for (const auto &point: perf.data) {
                scores.push_back(point.score);
            }
            return scores;
        }

        std::vector<float> get_accuracies(const std::string_view filename) override {
            const domain::ScenarioPerf perf = get_data(filename);
            std::vector<float> accuracies;
            for (const auto &point: perf.data) {
                if (point.shots == 0) accuracies.push_back(0);
                accuracies.push_back(static_cast<float>(point.hits) / static_cast<float>(point.shots));
            }
            return accuracies;
        }

    private:

        [[nodiscard]] domain::ScenarioPerf get_data(std::string_view filename) const {
            return m_decoder.decode_file(filename);
        }
        IProtoDecoder& m_decoder;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_USE_CASE_H
