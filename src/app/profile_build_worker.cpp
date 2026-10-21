//
// Created by Lecka on 12/08/2026.
//

#include "profile_build_worker.h"


namespace ksv::application {
    ProfileBuildWorker::ProfileBuildWorker(std::shared_ptr<data::IProfileImporter> importer, QObject *parent)
        : QObject(parent), m_importer(std::move(importer)) {
    }

    void ProfileBuildWorker::build(ProfileImportRequest request) {
        // One queued signal per file would post thousands of events at a directory of
        // any size, all to move a bar by less than a pixel. Whole percent is the
        // finest step the bar can actually show.
        int last_percent = -1;
        auto report = [this, &last_percent](ProfileImportPhase phase, const std::size_t done, const std::size_t total) {
            const int percent = total == 0 ? 100 : static_cast<int>(done * 100 / total);
            if (percent == last_percent && done != total) return;
            last_percent = percent;
            emit progress(phase, static_cast<int>(done), static_cast<int>(total));
        };

        try {
            emit finished(m_importer->run(request, report));
        } catch (const std::exception &error) {
            ProfileImportResult result;
            result.token = request.token;
            result.failed = true;
            result.summary.issues.push_back({{}, error.what(), false});
            emit finished(std::move(result));
        } catch (...) {
            ProfileImportResult result;
            result.token = request.token;
            result.failed = true;
            result.summary.issues.push_back({{}, "Unexpected import failure", false});
            emit finished(std::move(result));
        }
    }
}
