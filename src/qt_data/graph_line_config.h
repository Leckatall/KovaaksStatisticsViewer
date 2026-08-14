#ifndef KOVAAKSSTATSVIEWER_GRAPH_LINE_CONFIG_H
#define KOVAAKSSTATSVIEWER_GRAPH_LINE_CONFIG_H

#include <QMutex>
#include <QSettings>

#include "data/interfaces/i_graph_line_config.h"

namespace ksv::qt_data {
    class GraphLineConfig final : public application::IGraphLineConfig {
    public:
        explicit GraphLineConfig(QSettings::Format format = QSettings::NativeFormat);

        [[nodiscard]] std::vector<std::string> getDisabledGraphLineKeys() const override;
        void setDisabledGraphLineKeys(const std::vector<std::string> &keys) override;
        void onDisabledGraphLinesChanged(std::function<void()> callback) override;

    private:
        QSettings m_settings;
        mutable QMutex m_mutex;
        std::vector<std::function<void()>> m_callbacks;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_LINE_CONFIG_H
