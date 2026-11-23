#include "recent_benchmarks.h"

#include <algorithm>
#include <utility>

namespace ksv::application {
    RecentBenchmarks::RecentBenchmarks(std::shared_ptr<ISettingsService> settings)
        : m_settings(std::move(settings)), m_filenames(m_settings->getRecentBenchmarkFiles()) {}

    void RecentBenchmarks::markViewed(const std::string &filename) {
        if (!m_filenames.empty() && m_filenames.front() == filename) return;
        auto next = m_filenames;
        std::erase(next, filename);
        next.insert(next.begin(), filename);
        commit(std::move(next));
    }

    void RecentBenchmarks::markCreated(const std::string &filename) {
        if (std::ranges::find(m_filenames, filename) != m_filenames.end()) return;
        auto next = m_filenames;
        next.insert(next.begin() + (next.empty() ? 0 : 1), filename);
        commit(std::move(next));
    }

    void RecentBenchmarks::rename(const std::string &from, const std::string &to) {
        if (std::ranges::find(m_filenames, from) == m_filenames.end()) return;
        std::vector<std::string> next;
        for (const auto &filename: m_filenames) {
            if (filename == from) next.push_back(to);
            else if (filename != to) next.push_back(filename);
        }
        commit(std::move(next));
    }

    void RecentBenchmarks::commit(std::vector<std::string> next) {
        m_filenames = std::move(next);
        m_settings->setRecentBenchmarkFiles(m_filenames);
        for (const auto &callback: m_callbacks) callback();
    }
}
