//
// Created by Lecka on 12/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PROFILE_BUILD_WORKER_H
#define KOVAAKSSTATSVIEWER_PROFILE_BUILD_WORKER_H

#include <memory>

#include <QObject>

#include "user_profile.h"
#include "data/interfaces/i_file_service.h"
#include "data/interfaces/i_run_ingestor.h"
#include "data/interfaces/i_profile_importer.h"

// UserProfile crosses a thread boundary as a queued signal argument, so Qt needs a
// metatype for it. It is declared here rather than beside the type because domain
// stays free of Qt.
Q_DECLARE_METATYPE(ksv::domain::UserProfile)
Q_DECLARE_METATYPE(ksv::application::ProfileImportRequest)
Q_DECLARE_METATYPE(ksv::application::ProfileImportResult)
Q_DECLARE_METATYPE(ksv::application::ProfileImportPhase)

namespace ksv::application {
    class ProfileBuildWorker : public QObject {
        Q_OBJECT

    public:
        explicit ProfileBuildWorker(std::shared_ptr<data::IProfileImporter> importer, QObject *parent = nullptr);

    public slots:
        void build(ProfileImportRequest request);

    signals:
        void progress(ProfileImportPhase phase, int done, int total);
        void finished(ProfileImportResult result);

    private:
        std::shared_ptr<data::IProfileImporter> m_importer;
    };
}

#endif //KOVAAKSSTATSVIEWER_PROFILE_BUILD_WORKER_H
