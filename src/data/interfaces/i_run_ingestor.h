#ifndef KOVAAKSSTATSVIEWER_I_RUN_INGESTOR_H
#define KOVAAKSSTATSVIEWER_I_RUN_INGESTOR_H

#include <optional>

#include "interfaces/i_file_service.h"
#include "user_profile.h"

namespace ksv::data {
    class IRunIngestor {
    public:
        virtual ~IRunIngestor() = default;
        [[nodiscard]] virtual std::optional<domain::Run> buildRun(
            domain::UserProfile &profile, std::optional<application::PerfFile> perf,
            std::optional<application::StatsFile> csv) const = 0;
        [[nodiscard]] virtual std::optional<domain::Run> buildLiveRun(
            domain::UserProfile &profile, const application::PerfFile &perf) const = 0;
        [[nodiscard]] virtual std::optional<domain::Run> enrichStoredRun(
            domain::UserProfile &profile, domain::Run run) const = 0;
    };
}

#endif
