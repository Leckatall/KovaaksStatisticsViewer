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

// UserProfile crosses a thread boundary as a queued signal argument, so Qt needs a
// metatype for it. It is declared here rather than beside the type because domain
// stays free of Qt.
Q_DECLARE_METATYPE(ksv::domain::UserProfile)

namespace ksv::application {
    class ProfileBuildWorker : public QObject {
        Q_OBJECT

    public:
        ProfileBuildWorker(std::shared_ptr<IFileService> file_service,
                           std::shared_ptr<data::IRunIngestor> ingestor, QObject *parent = nullptr);

    public slots:
        void build();

    signals:
        void progress(int done, int total);
        void finished(domain::UserProfile profile);

    private:
        std::shared_ptr<IFileService> m_file_service;
        std::shared_ptr<data::IRunIngestor> m_ingestor;
    };
}

#endif //KOVAAKSSTATSVIEWER_PROFILE_BUILD_WORKER_H
