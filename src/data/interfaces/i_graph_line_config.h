#ifndef KOVAAKSSTATSVIEWER_I_GRAPH_LINE_CONFIG_H
#define KOVAAKSSTATSVIEWER_I_GRAPH_LINE_CONFIG_H

#include <functional>
#include <string>
#include <vector>

namespace ksv::application {
    class IGraphLineConfig {
    public:
        virtual ~IGraphLineConfig() = default;

        [[nodiscard]] virtual std::vector<std::string> getDisabledGraphLineKeys() const = 0;
        virtual void setDisabledGraphLineKeys(const std::vector<std::string> &keys) = 0;
        virtual void onDisabledGraphLinesChanged(std::function<void()> callback) = 0;
    };
}

#endif //KOVAAKSSTATSVIEWER_I_GRAPH_LINE_CONFIG_H
