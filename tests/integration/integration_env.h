//
// Shared helpers for the integration suite: a temp KovaaKs directory laid out
// the way FileService expects (<root>/FPSAimTrainer/performances/*.perf) plus a
// real IniFormat SettingsService pointed at it, so every test drives the real
// services with deterministic, throwaway paths.
//

#ifndef KSV_INTEGRATION_ENV_H
#define KSV_INTEGRATION_ENV_H

#include <memory>
#include <string>

#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include "file_service.h"
#include "formats/protobuf/profile_serializer.h"
#include "formats/protobuf/proto_decoder.h"
#include "profile_service.h"
#include "run_ingestor.h"
#include "kovaaks_dir.h"
#include "series_config_store.h"
#include "settings_service.h"

namespace ksv::integration {
    inline QString fixturePath(const QString &name) {
        return QDir(TEST_FILES_DIR).absoluteFilePath(name);
    }

    // The FileService -> RunIngestor -> ProfileService chain App::App() builds,
    // with every handle kept so tests can drive the layers individually.
    struct ProfileStack {
        std::shared_ptr<data::ProtoDecoder> decoder;
        std::shared_ptr<qt_data::FileService> fileService;
        std::shared_ptr<data::RunIngestor> ingestor;
        std::shared_ptr<data::ProfileSerializer> serializer;
        std::shared_ptr<data::ProfileService> profileService;
    };

    // Owns a temp dir shaped like a real KovaaKs install and a SettingsService
    // (IniFormat, already redirected to a temp registry by the suite main).
    struct TestEnv {
        tests_support::KovaaksDir kovaaks;
        std::shared_ptr<qt_data::SettingsService> settings =
            std::make_shared<qt_data::SettingsService>(QSettings::IniFormat);
        std::shared_ptr<qt_data::SeriesConfigStore> seriesConfigStore =
            std::make_shared<qt_data::SeriesConfigStore>(settings);

        TestEnv() {
            settings->setKovaaksDirs({rootPath().toStdString()});
            settings->setProfilePath(profileStorePath().toStdString());
        }

        [[nodiscard]] bool valid() const { return kovaaks.valid(); }
        [[nodiscard]] QString rootPath() const { return kovaaks.root(); }

        // A fresh stack each call: the reload tests need a second one built
        // over the same settings and store path.
        [[nodiscard]] ProfileStack makeProfileStack() const {
            auto decoder = std::make_shared<data::ProtoDecoder>();
            auto fileService = std::make_shared<qt_data::FileService>(settings, decoder);
            auto ingestor = std::make_shared<data::RunIngestor>(fileService);
            auto serializer = std::make_shared<data::ProfileSerializer>();
            return {decoder, fileService, ingestor, serializer,
                    std::make_shared<data::ProfileService>(fileService, serializer, settings, ingestor)};
        }

        [[nodiscard]] QString performancesDir() const { return kovaaks.performancesDir(); }
        [[nodiscard]] QString statsDir() const { return kovaaks.statsDir(); }
        [[nodiscard]] bool makePerformancesDir() const { return kovaaks.makePerformancesDir(); }
        [[nodiscard]] bool makeStatsDir() const { return kovaaks.makeStatsDir(); }

        [[nodiscard]] QString profileStorePath() const {
            return QDir(rootPath()).absoluteFilePath("store/profile.pb");
        }

        [[nodiscard]] QString copyFixtureIntoPerformances(const QString &fixtureName) const {
            return kovaaks.copyIntoPerformances(fixturePath(fixtureName));
        }

        [[nodiscard]] QString copyFixtureIntoStats(const QString &fixtureName,
                                                   const QString &destName = {}) const {
            return kovaaks.copyIntoStats(fixturePath(fixtureName), destName);
        }
    };
}

#endif //KSV_INTEGRATION_ENV_H
