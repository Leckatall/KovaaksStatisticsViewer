//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PLAYTIME_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_PLAYTIME_GRAPH_USE_CASE_H
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "contracts/i_graph_configuration_use_case.h"
#include "contracts/i_playtime_graph_use_case.h"
#include "data/interfaces/i_day_source.h"
#include "data/interfaces/i_profile_service.h"

namespace ksv::application {
    class PlaytimeGraphUseCase final : public IPlaytimeGraphUseCase {
    public:
        PlaytimeGraphUseCase(std::shared_ptr<IProfileService> profileService,
                             std::shared_ptr<IGraphConfigurationUseCase> configuration,
                             std::shared_ptr<data::IDaySource> daySource);

        [[nodiscard]] PlaytimeGraphSnapshot snapshot() override;
        void onChanged(std::function<void()> callback) override { m_callbacks.push_back(std::move(callback)); }

    private:
        void notifyChanged() const;
        [[nodiscard]] std::optional<std::chrono::sys_days> latestProfileDay() const;

        std::shared_ptr<IProfileService> m_profileService;
        std::shared_ptr<IGraphConfigurationUseCase> m_configuration;
        std::shared_ptr<data::IDaySource> m_daySource;
        std::vector<std::function<void()> > m_callbacks;

        // Full-history measurements for one window; a profile change drops them, a range or style
        // change leaves them alone.
        std::optional<int> m_fullWindowDays;
        PlaytimeSeries m_full;
        bool m_anchorValid = false;
        std::optional<std::chrono::sys_days> m_anchor;
    };
}

#endif //KOVAAKSSTATSVIEWER_PLAYTIME_GRAPH_USE_CASE_H
