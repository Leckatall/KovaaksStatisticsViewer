#ifndef KOVAAKSSTATISTICSVIEWER_I_COMPLETION_HISTORY_USE_CASE_H
#define KOVAAKSSTATISTICSVIEWER_I_COMPLETION_HISTORY_USE_CASE_H

#include <functional>
#include <optional>

#include "completion_history.h"

namespace ksv::application {
    class ICompletionHistoryUseCase {
    public:
        virtual ~ICompletionHistoryUseCase() = default;

        virtual CompletionHistory get_history() = 0;
        virtual void onCurrentScenarioChanged(std::function<void()> callback) = 0;

        // The selected run's CompletionRow::run_index in get_history(), or nullopt when no run is
        // selected or it is not in the history.
        virtual std::optional<int> selected_run_index() = 0;
        // Fires on every selected-run change, including one within the same scenario.
        virtual void onSelectedRunChanged(std::function<void()> callback) = 0;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_I_COMPLETION_HISTORY_USE_CASE_H
