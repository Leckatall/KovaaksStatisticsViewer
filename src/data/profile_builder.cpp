//
// Created by Lecka on 12/08/2026.
//

#include "profile_builder.h"

#include <exception>
#include <iostream>

namespace ksv::data {
    ProfileBuilder::ProfileBuilder(std::shared_ptr<application::IFileService> file_service)
        : m_file_service(std::move(file_service)) {
    }

    domain::UserProfile ProfileBuilder::build(const ProgressCallback &on_progress) const {
        const auto files = m_file_service->listPerfFiles();
        domain::UserProfile profile;
        for (const auto &root : m_file_service->sourceRoots()) {
            profile.ensureSource(root, "FPSAimTrainer/performances");
        }
        std::size_t done = 0;
        for (const auto &file: files) {
            try {
                auto perf = m_file_service->getPerfFromFile(file.absolutePath());
                perf.source = {profile.ensureSource(file.root, file.subdir), file.filename};
                profile.addScenarioPerf(perf);
            } catch (const std::exception &e) {
                std::cerr << "Skipping " << file.absolutePath() << ": " << e.what() << std::endl;
            }
            ++done;
            if (on_progress) on_progress(done, files.size());
        }
        return profile;
    }
}
