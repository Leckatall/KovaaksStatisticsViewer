//
// Created by Lecka on 03/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SETTINGS_VM_H
#define KOVAAKSSTATSVIEWER_SETTINGS_VM_H

#include <QtCore>
#include <QColor>

#include "data/interfaces/i_profile_service.h"
#include "data/interfaces/i_settings_service.h"
#include "app/contracts/i_series_management_use_case.h"

namespace ksv::presentation {
    class SettingsViewModel : public QObject {
        Q_OBJECT
        Q_PROPERTY(QUrl kovaaksDir READ getKovaaksDir WRITE setKovaaksDir NOTIFY kovaaksDirChanged)
        Q_PROPERTY(bool kovaaksDirSet READ isKovaaksDirSet NOTIFY kovaaksDirChanged)
        Q_PROPERTY(QUrl profilePath READ getProfilePath WRITE setProfilePath NOTIFY profilePathChanged)
        Q_PROPERTY(bool profileLoaded READ isProfileLoaded NOTIFY profileLoadedChanged)
        Q_PROPERTY(QVariantList allSeriesConfigs READ getAllSeriesConfigs NOTIFY seriesConfigurationChanged)
        Q_PROPERTY(bool pendingChanges READ hasPendingChanges NOTIFY pendingChangesChanged)

    public:
        explicit SettingsViewModel(std::shared_ptr<application::ISettingsService> settings_service,
                                   std::shared_ptr<application::IProfileService> profile_service,
                                   std::shared_ptr<application::ISeriesManagementUseCase> series_management,
                                   QObject *parent = nullptr);

        Q_INVOKABLE [[nodiscard]] QUrl getKovaaksDir() const { return m_kovaaks_dir; }
        [[nodiscard]] bool isKovaaksDirSet() const { return m_settings_service->isKovaaksDirSet(); }

        Q_INVOKABLE void setKovaaksDir(const QUrl &dir);

        Q_INVOKABLE [[nodiscard]] QUrl getProfilePath() const {
            return QUrl::fromLocalFile(m_settings_service->getProfilePath().data());
        }

        Q_INVOKABLE void setProfilePath(const QUrl &path);

        [[nodiscard]] bool isProfileLoaded() const { return m_profile_service->isProfileLoaded(); }
        Q_INVOKABLE [[nodiscard]] QVariantList getAllSeriesConfigs() const;
        Q_INVOKABLE QVariantMap setSeriesEnabled(const QString &id, bool enabled);
        Q_INVOKABLE QVariantMap createComputedSeries(const QString &name, const QColor &color, double width,
                                                      bool enabled, const QVariantMap &expression);
        Q_INVOKABLE QVariantMap updateComputedSeries(const QString &id, const QString &name, const QColor &color,
                                                      double width, bool enabled, const QVariantMap &expression);
        Q_INVOKABLE QVariantMap removeComputedSeries(const QString &id);
        Q_INVOKABLE QVariantMap reorderSeries(const QString &id, int displayPosition);

        [[nodiscard]] bool hasPendingChanges() const { return m_series_management->hasPendingChanges(); }
        Q_INVOKABLE void beginSeriesDraft() { m_series_management->beginDraft(); }
        Q_INVOKABLE QVariantMap commitSeriesDraft();
        Q_INVOKABLE void discardSeriesDraft();

    signals:
        void kovaaksDirChanged();
        void profilePathChanged();
        void profileLoadedChanged();
        void seriesConfigurationChanged();
        void pendingChangesChanged();

    private:
        std::shared_ptr<application::ISettingsService> m_settings_service;
        std::shared_ptr<application::IProfileService> m_profile_service;
        std::shared_ptr<application::ISeriesManagementUseCase> m_series_management;
        QUrl m_kovaaks_dir;
    };
}

#endif //KOVAAKSSTATSVIEWER_SETTINGS_VM_H
