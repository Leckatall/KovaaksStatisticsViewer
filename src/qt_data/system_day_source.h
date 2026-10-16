#pragma once

#include <chrono>
#include <functional>
#include <vector>

#include <QObject>
#include <QTimer>
#include <Qt>

#include "interfaces/i_day_source.h"

namespace ksv::qt_data {
    // Polls the UTC calendar day on the event loop and re-checks whenever the application becomes
    // active, because a suspended machine neither fires timers on time nor tells the app it slept.
    class SystemDaySource : public QObject, public data::IDaySource {
        Q_OBJECT
    public:
        using Clock = std::function<std::chrono::sys_days()>;

        static constexpr std::chrono::milliseconds kDefaultCheckInterval{30'000};

        [[nodiscard]] static std::chrono::sys_days utcToday();

        explicit SystemDaySource(Clock clock = &SystemDaySource::utcToday,
                                 std::chrono::milliseconds checkInterval = kDefaultCheckInterval,
                                 QObject *parent = nullptr);

        [[nodiscard]] std::chrono::sys_days today() const override { return m_observed; }

        void onDayChanged(std::function<void(std::chrono::sys_days)> callback) override {
            m_callbacks.push_back(std::move(callback));
        }

        // Observes the clock now; announces only when the day differs from the last observed one.
        void refresh();

        // Public so it can be driven without a QGuiApplication.
        void onApplicationStateChanged(Qt::ApplicationState state);

    private:
        Clock m_clock;
        std::chrono::sys_days m_observed;
        QTimer m_timer;
        std::vector<std::function<void(std::chrono::sys_days)>> m_callbacks;
    };
}
