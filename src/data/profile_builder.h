//
// Created by Lecka on 12/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PROFILE_BUILDER_H
#define KOVAAKSSTATSVIEWER_PROFILE_BUILDER_H

#include <cstddef>
#include <functional>
#include <memory>

#include "user_profile.h"
#include "interfaces/i_file_service.h"
#include "interfaces/i_run_ingestor.h"

namespace ksv::data {
    class ProfileBuilder {
    public:
        // Called after each group is folded in, with (groups done, groups total).
        using ProgressCallback = std::function<void(std::size_t, std::size_t)>;

        ProfileBuilder(std::shared_ptr<application::IFileService> file_service,
                       std::shared_ptr<IRunIngestor> ingestor);

        [[nodiscard]] domain::UserProfile build(const ProgressCallback& on_progress = {}) const;

    private:
        // Held by value rather than by reference so a copy of the builder can outlive
        // the caller's stack frame once build() runs on a worker thread.
        std::shared_ptr<application::IFileService> m_file_service;
        std::shared_ptr<IRunIngestor> m_ingestor;
    };
}

#endif //KOVAAKSSTATSVIEWER_PROFILE_BUILDER_H
