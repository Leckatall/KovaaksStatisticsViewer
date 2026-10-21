#ifndef KOVAAKSSTATSVIEWER_I_RUN_INGESTOR_H
#define KOVAAKSSTATSVIEWER_I_RUN_INGESTOR_H

#include <optional>

#include "interfaces/i_file_service.h"
#include "user_profile.h"

namespace ksv::data {
    struct IngestResult {
        std::optional<domain::Run> run;
        std::vector<application::ProfileImportIssue> issues;
    };
    class IRunIngestor {
    public:
        virtual ~IRunIngestor() = default;
        [[nodiscard]] virtual IngestResult ingestRun(domain::UserProfile &, std::optional<application::PerfFile>,
            std::optional<application::StatsFile>) const = 0;
        [[nodiscard]] virtual IngestResult ingestSibling(domain::UserProfile &, domain::Run,
            std::optional<application::PerfFile>, std::optional<application::StatsFile>) const = 0;
        [[nodiscard]] virtual std::optional<domain::Run> buildRun(
            domain::UserProfile &profile, std::optional<application::PerfFile> perf,
            std::optional<application::StatsFile> csv) const = 0;
        [[nodiscard]] virtual std::optional<domain::Run> enrichStoredRun(
            domain::UserProfile &profile, domain::Run run) const = 0;
    };
}

#endif
