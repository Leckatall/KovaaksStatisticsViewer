#include "recent_benchmarks.h"

#include <algorithm>
#include <utility>

namespace ksv::application {
    RecentBenchmarks::RecentBenchmarks(std::shared_ptr<ISettingsService> settings)
        : m_settings(std::move(settings)), m_ids(m_settings->getRecentBenchmarkIds()) {}

    void RecentBenchmarks::markViewed(const std::string &id) {
        if (!m_ids.empty() && m_ids.front() == id) return;
        auto next = m_ids;
        std::erase(next, id);
        next.insert(next.begin(), id);
        commit(std::move(next));
    }

    void RecentBenchmarks::markCreated(const std::string &id) {
        if (std::ranges::find(m_ids, id) != m_ids.end()) return;
        auto next = m_ids;
        next.insert(next.begin() + (next.empty() ? 0 : 1), id);
        commit(std::move(next));
    }

    void RecentBenchmarks::commit(std::vector<std::string> next) {
        m_ids = std::move(next);
        m_settings->setRecentBenchmarkIds(m_ids);
        for (const auto &callback: m_callbacks) callback();
    }
}
