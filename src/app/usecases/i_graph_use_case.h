//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_I_GRAPH_USE_CASE_H
#include <string>


namespace ksv::application {
    class IGraphUseCase {
    public:
        virtual ~IGraphUseCase() = default;

        virtual std::vector<float> get_times(std::string_view filename) = 0;

        virtual std::vector<float> get_scores(std::string_view filename) = 0;

        virtual std::vector<float> get_accuracies(std::string_view filename) = 0;
    };
}


#endif //KOVAAKSSTATSVIEWER_I_GRAPH_USE_CASE_H
