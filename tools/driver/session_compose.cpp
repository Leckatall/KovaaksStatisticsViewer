#include "session_compose.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QUrl>
#include <string>
#include <vector>

#include "settings_service.h"

namespace ksv::driver {
    namespace {
        // The URL-encoded keys SettingsService reads (src/qt_data/settings_service.cpp).
        constexpr auto kKovaaksDirsKey = "file/kovaaksDirs";
        constexpr auto kLegacyKovaaksKey = "file/kovaaks";
        constexpr auto kProfilePathKey = "file/profilePath";

        // Writes (or, for nullopt, removes) one key in the session's settings file,
        // the one SettingsService and the QML Settings {} objects share.
        bool writeRaw(const QString &key, const std::optional<QVariant> &value) {
            QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Lecka"),
                               QStringLiteral("KovaaksStatsViewer"));
            if (value) settings.setValue(key, *value);
            else settings.remove(key);
            settings.sync();
            return settings.status() == QSettings::NoError;
        }

        // A path value: a JSON string, or an unparsed string.
        bool pathValue(const QVariant &value, QString &path) {
            if (value.typeId() != QMetaType::QString) return false;
            path = value.toString();
            return true;
        }
    }

    QString applyOverlay(const QString &overlayDir, const QString &dataRoot) {
        const QFileInfo overlay(overlayDir);
        if (!overlay.isDir()) return QStringLiteral("overlay '%1' is not a directory").arg(overlayDir);
        const QDir source(overlay.absoluteFilePath());
        QDirIterator it(source.absolutePath(), QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString file = it.next();
            const QString relative = source.relativeFilePath(file);
            const QString target = QDir(dataRoot).filePath(relative);
            if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
                return QStringLiteral("overlay '%1': could not create the folder for %2").arg(overlayDir, relative);
            }
            if (QFileInfo::exists(target) && !QFile::remove(target)) {
                return QStringLiteral("overlay '%1': could not replace %2").arg(overlayDir, relative);
            }
            if (!QFile::copy(file, target)) {
                return QStringLiteral("overlay '%1': could not copy %2").arg(overlayDir, relative);
            }
            // Copies keep the source's permissions; the session must be able to edit them.
            QFile::setPermissions(target, QFile::permissions(target) | QFileDevice::WriteOwner | QFileDevice::WriteUser);
        }
        return {};
    }

    QString parseSettingAssignment(const QString &text, SettingAssignment &out) {
        const auto equals = text.indexOf(u'=');
        if (equals < 0) return QStringLiteral("--setting '%1' must look like KEY=VALUE").arg(text);
        const QString key = text.left(equals).trimmed();
        if (key.isEmpty()) return QStringLiteral("--setting '%1' has an empty key").arg(text);
        const QString raw = text.mid(equals + 1);
        out.key = key;
        if (raw.isEmpty()) {
            out.value.reset();
            return {};
        }
        // Wrapping in [] lets QJsonDocument parse scalars too.
        QJsonParseError error{};
        const QJsonDocument document = QJsonDocument::fromJson(QStringLiteral("[%1]").arg(raw).toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && document.array().size() == 1) {
            out.value = document.array().first().toVariant();
        } else {
            out.value = raw;
        }
        return {};
    }

    QString applySetting(const SettingAssignment &assignment, qt_data::SettingsService &service) {
        const QString &key = assignment.key;
        if (!assignment.value) {
            if (!writeRaw(key, std::nullopt)) return QStringLiteral("--setting %1: could not remove it").arg(key);
            return {};
        }
        const QVariant &value = *assignment.value;
        if (key == QLatin1String(kKovaaksDirsKey)) {
            std::vector<std::string> dirs;
            QString path;
            if (pathValue(value, path)) {
                dirs.push_back(path.toStdString());
            } else if (value.typeId() == QMetaType::QVariantList) {
                for (const auto &element: value.toList()) {
                    if (!pathValue(element, path)) {
                        return QStringLiteral("--setting %1: expected a path or a JSON array of paths").arg(key);
                    }
                    dirs.push_back(path.toStdString());
                }
            } else {
                return QStringLiteral("--setting %1: expected a path or a JSON array of paths").arg(key);
            }
            service.setKovaaksDirs(dirs);
            return {};
        }
        if (key == QLatin1String(kProfilePathKey) || key == QLatin1String(kLegacyKovaaksKey)) {
            QString path;
            if (!pathValue(value, path)) return QStringLiteral("--setting %1: expected a path").arg(key);
            if (key == QLatin1String(kProfilePathKey)) {
                service.setProfilePath(path.toStdString());
            } else if (!writeRaw(key, QVariant(QUrl::fromLocalFile(path)))) {
                return QStringLiteral("--setting %1: could not write it").arg(key);
            }
            return {};
        }
        if (!writeRaw(key, value)) return QStringLiteral("--setting %1: could not write it").arg(key);
        return {};
    }
}
