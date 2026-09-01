#ifndef KOVAAKSSTATSVIEWER_TESTS_KOVAAKS_DIR_H
#define KOVAAKSSTATSVIEWER_TESTS_KOVAAKS_DIR_H

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>

namespace ksv::tests_support {
    // A throwaway directory laid out the way FileService expects a KovaaK's install to
    // be: <root>/FPSAimTrainer/{performances,stats}. Takes absolute source paths rather
    // than fixture names so it stays independent of each suite's own TEST_FILES_DIR.
    struct KovaaksDir {
        QTemporaryDir dir;

        [[nodiscard]] bool valid() const { return dir.isValid(); }
        [[nodiscard]] QString root() const { return dir.path(); }

        [[nodiscard]] QString performancesDir() const {
            return QDir(dir.path()).absoluteFilePath("FPSAimTrainer/performances");
        }

        [[nodiscard]] QString statsDir() const {
            return QDir(dir.path()).absoluteFilePath("FPSAimTrainer/stats");
        }

        [[nodiscard]] bool makePerformancesDir() const { return QDir().mkpath(performancesDir()); }
        [[nodiscard]] bool makeStatsDir() const { return QDir().mkpath(statsDir()); }

        // Both return the copied file's absolute path, or empty on failure. destName
        // lets a stats copy land on the ` Stats.csv` sibling name a live perf expects.
        [[nodiscard]] QString copyIntoPerformances(const QString &sourcePath, const QString &destName = {}) const {
            return copyInto(performancesDir(), sourcePath, destName);
        }

        [[nodiscard]] QString copyIntoStats(const QString &sourcePath, const QString &destName = {}) const {
            return copyInto(statsDir(), sourcePath, destName);
        }

        [[nodiscard]] bool writeIntoStats(const QString &name, const QByteArray &bytes) const {
            QFile file(QDir(statsDir()).absoluteFilePath(name));
            if (!file.open(QIODevice::WriteOnly)) return false;
            return file.write(bytes) == bytes.size();
        }

    private:
        [[nodiscard]] static QString copyInto(const QString &targetDir, const QString &sourcePath,
                                              const QString &destName) {
            const QString name = destName.isEmpty() ? QFileInfo(sourcePath).fileName() : destName;
            const QString dest = QDir(targetDir).absoluteFilePath(name);
            return QFile::copy(sourcePath, dest) ? dest : QString();
        }
    };
}

#endif
