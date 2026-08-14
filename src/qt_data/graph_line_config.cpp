#include "graph_line_config.h"

#include <QMutexLocker>
#include <QStringList>
#include <QSet>

namespace ksv::qt_data {
    namespace {
        constexpr auto kDisabledColumnsKey = "graph/disabledColumns";

        QStringList normalizedKeys(const std::vector<std::string> &keys) {
            QStringList result;
            QSet<QString> seen;
            result.reserve(static_cast<qsizetype>(keys.size()));
            for (const auto &key: keys) {
                const auto value = QString::fromStdString(key);
                if (seen.contains(value)) continue;
                seen.insert(value);
                result.push_back(value);
            }
            return result;
        }
    }

    GraphLineConfig::GraphLineConfig(const QSettings::Format format)
        : m_settings(format, QSettings::UserScope, "Lecka", "KovaaksStatsViewer") {}

    std::vector<std::string> GraphLineConfig::getDisabledGraphLineKeys() const {
        const QMutexLocker locker(&m_mutex);
        const auto stored = m_settings.value(kDisabledColumnsKey).toStringList();
        std::vector<std::string> result;
        result.reserve(static_cast<std::size_t>(stored.size()));
        for (const auto &key: stored) result.push_back(key.toStdString());
        return result;
    }

    void GraphLineConfig::setDisabledGraphLineKeys(const std::vector<std::string> &keys) {
        std::vector<std::function<void()>> callbacks;
        {
            const QMutexLocker locker(&m_mutex);
            const auto normalized = normalizedKeys(keys);
            if (m_settings.value(kDisabledColumnsKey).toStringList() == normalized) return;
            m_settings.setValue(kDisabledColumnsKey, normalized);
            m_settings.sync();
            callbacks = m_callbacks;
        }
        for (const auto &callback: callbacks) callback();
    }

    void GraphLineConfig::onDisabledGraphLinesChanged(std::function<void()> callback) {
        const QMutexLocker locker(&m_mutex);
        m_callbacks.push_back(std::move(callback));
    }
}
