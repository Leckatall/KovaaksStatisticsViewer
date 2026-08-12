//
// Created by Lecka on 12/08/2026.
//

#include "profile_builder.h"

namespace ksv::data {
    ProfileBuilder::ProfileBuilder(std::shared_ptr<application::IFileService> file_service)
        : m_file_service(std::move(file_service)) {
    }

    domain::UserProfile ProfileBuilder::build(const ProgressCallback &on_progress) const {
        const auto files = m_file_service->listPerfFiles();
        domain::UserProfile profile{m_file_service->getSourceDirectory()};
        std::size_t done = 0;
        for (const auto &file: files) {
            profile.addScenarioPerf(m_file_service->getPerfFromFile(file));
            ++done;
            if (on_progress) on_progress(done, files.size());
        }
        return profile;
    }
}
