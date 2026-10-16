#ifndef KOVAAKSSTATISTICSVIEWER_I_COMPLETION_HISTORY_USE_CASE_H
#define KOVAAKSSTATISTICSVIEWER_I_COMPLETION_HISTORY_USE_CASE_H

#include <functional>
#include <optional>

#include "completion_history.h"

namespace ksv::application {
    class ICompletionHistoryUseCase {
    public:
        virtual ~ICompletionHistoryUseCase() = default;

        // The rows the committed range shows; run_index and selection always address the full history.
        virtual CompletionHistory get_history() = 0;
        virtual void onCurrentScenarioChanged(std::function<void()> callback) = 0;
        // Fires when the visible rows may differ without the scenario or profile changing: a committed
        // change to this graph's configuration, or a calendar rollover under a today-anchored range.
        virtual void onRangeChanged(std::function<void()> callback) = 0;

        // The selected run's CompletionRow::run_index, or nullopt when no run is selected or it is not
        // in the scenario's full history. The run may lie outside get_history()'s visible rows.
        virtual std::optional<int> selected_run_index() = 0;
        // Fires on every selected-run change, including one within the same scenario.
        virtual void onSelectedRunChanged(std::function<void()> callback) = 0;
        // Selects the full history's run with this run_index; an index outside it does nothing.
        virtual void select_run(int run_index) = 0;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_I_COMPLETION_HISTORY_USE_CASE_H
