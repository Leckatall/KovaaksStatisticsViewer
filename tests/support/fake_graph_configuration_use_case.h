#ifndef KOVAAKSSTATISTICSVIEWER_TESTS_FAKE_GRAPH_CONFIGURATION_USE_CASE_H
#define KOVAAKSSTATISTICSVIEWER_TESTS_FAKE_GRAPH_CONFIGURATION_USE_CASE_H

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "app/contracts/i_graph_configuration_use_case.h"

namespace ksv::tests_support {
    // Accepts every save and patch without validating; tests set `configuration` directly and call
    // publish() to simulate a committed change.
    class FakeGraphConfigurationUseCase final : public application::IGraphConfigurationUseCase {
    public:
        application::GraphConfiguration configuration = application::defaultGraphConfiguration();
        std::uint64_t revision = 1;
        std::vector<application::GraphConfiguration> saves;
        std::vector<std::pair<application::GraphKey, application::GraphQuickPatch>> patches;
        std::vector<std::function<void(const application::GraphConfigurationChange &)>> callbacks;

        [[nodiscard]] application::GraphConfigurationSnapshot current() const override {
            return {std::make_shared<const application::GraphConfiguration>(configuration), revision};
        }

        void onChanged(std::function<void(const application::GraphConfigurationChange &)> callback) override {
            callbacks.push_back(std::move(callback));
        }

        [[nodiscard]] application::GraphConfigurationEditorSeed editorSeed() const override {
            return {configuration, revision};
        }

        application::GraphConfigurationSaveResult save(const application::GraphConfiguration &candidate) override {
            saves.push_back(candidate);
            configuration = candidate;
            return {{}, ++revision};
        }

        application::GraphConfigurationSaveResult applyQuickPatch(const application::GraphKey graph,
                                                                  const application::GraphQuickPatch &patch) override {
            patches.emplace_back(graph, patch);
            return {{}, revision};
        }

        void publish(application::GraphConfigurationChange change) {
            change.revision = revision;
            for (const auto &callback: callbacks) callback(change);
        }
    };
}

#endif
