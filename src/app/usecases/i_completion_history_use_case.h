#ifndef KOVAAKSSTATISTICSVIEWER_I_COMPLETION_HISTORY_USE_CASE_H
#define KOVAAKSSTATISTICSVIEWER_I_COMPLETION_HISTORY_USE_CASE_H

#include <functional>

#include "completion_history.h"

namespace ksv::application {
    class ICompletionHistoryUseCase {
    public:
        virtual ~ICompletionHistoryUseCase() = default;

        virtual CompletionHistory get_history() = 0;
        virtual void onCurrentScenarioChanged(std::function<void()> callback) = 0;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_I_COMPLETION_HISTORY_USE_CASE_H
