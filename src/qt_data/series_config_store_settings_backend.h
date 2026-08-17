#pragma once

#include <QSettings>

namespace ksv::qt_data {
    class ISeriesConfigStoreSettingsBackend {
    public:
        virtual ~ISeriesConfigStoreSettingsBackend() = default;
        [[nodiscard]] virtual bool contains(const QString &key) const = 0;
        [[nodiscard]] virtual QVariant value(const QString &key) const = 0;
        virtual void setValue(const QString &key, const QVariant &value) = 0;
        virtual void sync() = 0;
        [[nodiscard]] virtual QSettings::Status status() const = 0;
        virtual void reload() = 0;
    };

    class QSettingsSeriesConfigStoreSettingsBackend final : public ISeriesConfigStoreSettingsBackend {
    public:
        explicit QSettingsSeriesConfigStoreSettingsBackend(QSettings::Format format = QSettings::NativeFormat);
        [[nodiscard]] bool contains(const QString &key) const override;
        [[nodiscard]] QVariant value(const QString &key) const override;
        void setValue(const QString &key, const QVariant &value) override;
        void sync() override;
        [[nodiscard]] QSettings::Status status() const override;
        void reload() override;

    private:
        QSettings m_settings;
    };
}
