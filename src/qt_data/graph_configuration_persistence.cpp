#include "graph_configuration_persistence.h"

#include <QJsonValue>
#include <QString>

#include <utility>

#include "graph_configuration_codec.h"
#include "series_config_codec.h"

namespace ksv::qt_data {
    using namespace application;

    GraphConfigurationPersistence::GraphConfigurationPersistence(std::shared_ptr<ISettingsService> settingsService)
        : m_settingsService(std::move(settingsService)) {
    }

    GraphConfiguration GraphConfigurationPersistence::load() {
        std::optional<DecodedGraphConfiguration> decoded;
        bool writeBack = !m_settingsService->hasSeriesConfigDocument();
        if (!writeBack) {
            const auto raw = m_settingsService->getSeriesConfigDocument();
            decoded = decodeGraphConfigurationDocument(raw);
            if (!decoded || decoded->hasRejectedSections) {
                m_settingsService->quarantineSeriesConfigDocument(raw);
                writeBack = true;
            }
        }
        if (!decoded) decoded = DecodedGraphConfiguration{defaultGraphConfiguration(), true, true, false};

        auto configuration = std::move(decoded->configuration);
        if (decoded->runSeriesRejected)
            configuration.runSeries = series_codec::seededRunSeries(m_settingsService->getLegacyDisabledColumnKeys());
        // VisualSettingsManager.qml's selection, until the document carries its own.
        if (!decoded->runAxisLabelStored) {
            const auto legacy = QString::fromStdString(m_settingsService->getLegacyRunAxisLabelSeriesId());
            if (const auto id = series_codec::decimalId(QJsonValue(legacy)))
                configuration.runAxisLabel = SeriesId{*id};
        }
        configuration.runAxisLabel = reconciledRunAxisLabel(configuration.runSeries, configuration.runAxisLabel);
        if (writeBack) save(configuration);
        return configuration;
    }

    void GraphConfigurationPersistence::save(const GraphConfiguration &configuration) {
        m_settingsService->setSeriesConfigDocument(encodeGraphConfigurationDocument(configuration));
    }
}
