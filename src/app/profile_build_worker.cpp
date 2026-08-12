//
// Created by Lecka on 12/08/2026.
//

#include "profile_build_worker.h"

#include "profile_builder.h"

namespace ksv::application {
    ProfileBuildWorker::ProfileBuildWorker(std::shared_ptr<IFileService> file_service, QObject *parent)
        : QObject(parent), m_file_service(std::move(file_service)) {
    }

    void ProfileBuildWorker::build() {
        // One queued signal per file would post thousands of events at a directory of
        // any size, all to move a bar by less than a pixel. Whole percent is the
        // finest step the bar can actually show.
        int last_percent = -1;
        auto report = [this, &last_percent](const std::size_t done, const std::size_t total) {
            const int percent = total == 0 ? 100 : static_cast<int>(done * 100 / total);
            if (percent == last_percent && done != total) return;
            last_percent = percent;
            emit progress(static_cast<int>(done), static_cast<int>(total));
        };

        emit finished(data::ProfileBuilder{m_file_service}.build(report));
    }
}
