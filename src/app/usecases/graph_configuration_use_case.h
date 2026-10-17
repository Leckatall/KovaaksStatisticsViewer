#pragma once

#include <memory>

#include "contracts/i_graph_configuration_use_case.h"
#include "data/interfaces/i_graph_configuration_service.h"

namespace ksv::application {
    class GraphConfigurationUseCase final : public IGraphConfigurationUseCase {
    public:
        explicit GraphConfigurationUseCase(std::shared_ptr<data::IGraphConfigurationService> service);

        [[nodiscard]] GraphConfigurationSnapshot current() const override;

        void onChanged(std::function<void(const GraphConfigurationChange &)> callback) override;

        GraphConfigurationSaveResult save(const GraphConfiguration &candidate) override;

        GraphConfigurationSaveResult applyQuickPatch(GraphKey graph, const GraphQuickPatch &patch) override;

    private:
        std::shared_ptr<data::IGraphConfigurationService> m_service;
    };
}
