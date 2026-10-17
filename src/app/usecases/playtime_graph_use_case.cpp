#include "playtime_graph_use_case.h"

#include <algorithm>
#include <utility>

namespace ksv::application {
    namespace {
        PlaytimeSeries includeInactiveDays(const PlaytimeSeries &samples) {
            if (samples.empty()) return {};

            PlaytimeSeries daily;
            daily.reserve(static_cast<std::size_t>((samples.back().first - samples.front().first).count()) + 1);
            auto nextDay = samples.front().first;
            for (const auto &[day, seconds]: samples) {
                for (; nextDay < day; nextDay += std::chrono::days{1})
                    daily.emplace_back(nextDay, 0.0);
                daily.emplace_back(day, seconds);
                nextDay = day + std::chrono::days{1};
            }
            return daily;
        }
    }

    PlaytimeGraphUseCase::PlaytimeGraphUseCase(std::shared_ptr<IProfileService> profileService,
                                               std::shared_ptr<IGraphConfigurationUseCase> configuration,
                                               std::shared_ptr<data::IDaySource> daySource)
        : m_profileService(std::move(profileService)), m_configuration(std::move(configuration)),
          m_daySource(std::move(daySource)), m_presentation(m_configuration, GraphKey::DailyPlaytime) {
        m_profileService->onProfileChanged([this] {
            m_fullWindowDays.reset();
            m_full.clear();
            m_anchorValid = false;
            notifyChanged();
        });
        m_configuration->onChanged([this](const GraphConfigurationChange &change) {
            const auto &sections = change.sectionsOf(GraphKey::DailyPlaytime);
            if (sections.range || sections.window) notifyChanged();
        });
        m_daySource->onDayChanged([this](std::chrono::sys_days) {
            const auto snapshot = m_configuration->current();
            if (followsToday(effectiveRange(snapshot.configuration->preferences(GraphKey::DailyPlaytime))))
                notifyChanged();
        });
    }

    PlaytimeGraphSnapshot PlaytimeGraphUseCase::snapshot() {
        const auto committed = m_configuration->current();
        const auto &preferences = committed.configuration->preferences(GraphKey::DailyPlaytime);
        const int window = effectiveAveragingWindow(preferences);
        if (m_fullWindowDays != window) {
            m_full = includeInactiveDays(m_profileService->getRollingTimeAverage(window));
            m_fullWindowDays = window;
        }
        if (!m_anchorValid) {
            m_anchor = latestProfileDay();
            m_anchorValid = true;
        }

        const auto projection = resolveRange(effectiveRange(preferences), m_daySource->today(), m_anchor);
        return {describePlaytimeView(window, projection, m_full), visiblePlaytime(m_full, projection)};
    }

    std::optional<std::chrono::sys_days> PlaytimeGraphUseCase::latestProfileDay() const {
        const auto latest = m_profileService->getLatestRun();
        // Matches the accounting, which skips runs stamped at or before the epoch.
        if (latest.run_id.start_time <= 0) return std::nullopt;
        return latest.run_id.startDay();
    }

    void PlaytimeGraphUseCase::notifyChanged() const {
        for (const auto &callback: m_callbacks) callback();
    }
}
