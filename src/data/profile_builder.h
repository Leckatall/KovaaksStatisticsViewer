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

namespace ksv::data {
    class ProfileBuilder {
    public:
        // Called after each file is folded in, with (files done, files total).
        using ProgressCallback = std::function<void(std::size_t, std::size_t)>;

        explicit ProfileBuilder(std::shared_ptr<application::IFileService> file_service);

        [[nodiscard]] domain::UserProfile build(const ProgressCallback& on_progress = {}) const;

    private:
        // Held by value rather than by reference so a copy of the builder can outlive
        // the caller's stack frame once build() runs on a worker thread.
        std::shared_ptr<application::IFileService> m_file_service;
    };
}

#endif //KOVAAKSSTATSVIEWER_PROFILE_BUILDER_H
