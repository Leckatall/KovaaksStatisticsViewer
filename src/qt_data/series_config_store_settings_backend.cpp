#include "series_config_store_settings_backend.h"

namespace ksv::qt_data {
    QSettingsSeriesConfigStoreSettingsBackend::QSettingsSeriesConfigStoreSettingsBackend(const QSettings::Format format)
        : m_settings(format, QSettings::UserScope, "Lecka", "KovaaksStatsViewer") {}

    bool QSettingsSeriesConfigStoreSettingsBackend::contains(const QString &key) const { return m_settings.contains(key); }
    QVariant QSettingsSeriesConfigStoreSettingsBackend::value(const QString &key) const { return m_settings.value(key); }
    void QSettingsSeriesConfigStoreSettingsBackend::setValue(const QString &key, const QVariant &value) { m_settings.setValue(key, value); }
    void QSettingsSeriesConfigStoreSettingsBackend::sync() { m_settings.sync(); }
    QSettings::Status QSettingsSeriesConfigStoreSettingsBackend::status() const { return m_settings.status(); }
    void QSettingsSeriesConfigStoreSettingsBackend::reload() { m_settings.sync(); }
}
