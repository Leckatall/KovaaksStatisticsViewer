//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_I_GRAPH_USE_CASE_H
#include <functional>
#include <string>

#include "perf_column_builder.h"

namespace ksv::application {
    class IGraphUseCase {
    public:
        virtual ~IGraphUseCase() = default;

        virtual void load_perf(std::string_view filename) = 0;

        virtual void load_latest_perf() = 0;

        virtual GraphSeries get_series() = 0;

        virtual std::string get_run_label() = 0;

        virtual void onCurrentPerfChanged(std::function<void()> callback) = 0;
    };
}


#endif //KOVAAKSSTATSVIEWER_I_GRAPH_USE_CASE_H
