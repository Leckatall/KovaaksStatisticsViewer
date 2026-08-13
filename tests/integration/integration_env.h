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

#include "settings_service.h"

namespace ksv::integration {
    inline QString fixturePath(const QString &name) {
        return QDir(TEST_FILES_DIR).absoluteFilePath(name);
    }

    // Owns a temp dir shaped like a real KovaaKs install and a SettingsService
    // (IniFormat, already redirected to a temp registry by the suite main).
    struct TestEnv {
        QTemporaryDir dir;
        std::shared_ptr<qt_data::SettingsService> settings =
            std::make_shared<qt_data::SettingsService>(QSettings::IniFormat);

        TestEnv() {
            settings->setKovaaksDir(dir.path().toStdString());
            settings->setProfilePath(profileStorePath().toStdString());
        }

        [[nodiscard]] bool valid() const { return dir.isValid(); }

        [[nodiscard]] QString performancesDir() const {
            return QDir(dir.path()).absoluteFilePath("FPSAimTrainer/performances");
        }

        [[nodiscard]] QString profileStorePath() const {
            return QDir(dir.path()).absoluteFilePath("store/profile.pb");
        }

        bool makePerformancesDir() const { return QDir().mkpath(performancesDir()); }

        // Returns the copied file's absolute path, or empty on failure.
        [[nodiscard]] QString copyFixtureIntoPerformances(const QString &fixtureName) const {
            const QString dest = QDir(performancesDir()).absoluteFilePath(fixtureName);
            return QFile::copy(fixturePath(fixtureName), dest) ? dest : QString();
        }
    };
}

#endif //KSV_INTEGRATION_ENV_H
