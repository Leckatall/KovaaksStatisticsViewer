#ifndef KOVAAKSSTATSVIEWER_RUN_INGESTOR_H
#define KOVAAKSSTATSVIEWER_RUN_INGESTOR_H

#include <memory>

#include "interfaces/i_run_ingestor.h"

namespace ksv::data {
    struct RunFileGroup {
        std::optional<application::PerfFile> perf;
        std::optional<application::StatsFile> csv;
    };

    [[nodiscard]] std::vector<RunFileGroup> pairRunFiles(
        const std::vector<application::PerfFile> &perfs, const std::vector<application::StatsFile> &csvs);

    class RunIngestor final : public IRunIngestor {
    public:
        explicit RunIngestor(std::shared_ptr<application::IFileService> file_service);
        [[nodiscard]] std::optional<domain::Run> buildRun(domain::UserProfile &, std::optional<application::PerfFile>, std::optional<application::StatsFile>) const override;
        [[nodiscard]] std::optional<domain::Run> buildLiveRun(domain::UserProfile &, const application::PerfFile &) const override;
        [[nodiscard]] std::optional<domain::Run> enrichStoredRun(domain::UserProfile &, domain::Run) const override;
    private:
        std::shared_ptr<application::IFileService> m_file_service;
    };
}

#endif
