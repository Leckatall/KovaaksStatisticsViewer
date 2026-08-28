//
// Created by Lecka on 12/08/2026.
//

#include "profile_builder.h"
#include "run_ingestor.h"

namespace ksv::data {
    ProfileBuilder::ProfileBuilder(std::shared_ptr<application::IFileService> file_service,
                                   std::shared_ptr<IRunIngestor> ingestor)
        : m_file_service(std::move(file_service)), m_ingestor(std::move(ingestor)) {
    }

    domain::UserProfile ProfileBuilder::build(const ProgressCallback &on_progress) const {
        const auto groups = pairRunFiles(m_file_service->listPerfFiles(), m_file_service->listStatsFiles());
        domain::UserProfile profile;
        for (const auto &root : m_file_service->sourceRoots()) {
            profile.ensureSource(root, "FPSAimTrainer/performances");
        }
        std::size_t done = 0;
        for (const auto &group: groups) {
            if (const auto run = m_ingestor->buildRun(profile, group.perf, group.csv)) profile.addRun(*run);
            ++done;
            if (on_progress) on_progress(done, groups.size());
        }
        return profile;
    }
}
