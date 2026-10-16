#ifndef KOVAAKSSTATISTICSVIEWER_TESTS_FAKE_DAY_SOURCE_H
#define KOVAAKSSTATISTICSVIEWER_TESTS_FAKE_DAY_SOURCE_H

#include <chrono>
#include <functional>
#include <utility>
#include <vector>

#include "data/interfaces/i_day_source.h"

namespace ksv::tests_support {
    class FakeDaySource final : public data::IDaySource {
    public:
        explicit FakeDaySource(std::chrono::sys_days start = std::chrono::sys_days{
                                   std::chrono::year{2026} / std::chrono::October / 16})
            : m_today(start) {}

        [[nodiscard]] std::chrono::sys_days today() const override { return m_today; }

        void onDayChanged(std::function<void(std::chrono::sys_days)> callback) override {
            m_callbacks.push_back(std::move(callback));
        }

        // Announces only when the day actually differs, like the real adapter.
        void setToday(std::chrono::sys_days day) {
            if (day == m_today) return;
            m_today = day;
            const auto callbacks = m_callbacks;
            for (const auto &callback : callbacks) callback(m_today);
        }

        void advance(int days = 1) { setToday(m_today + std::chrono::days{days}); }

    private:
        std::chrono::sys_days m_today;
        std::vector<std::function<void(std::chrono::sys_days)>> m_callbacks;
    };
}

#endif
