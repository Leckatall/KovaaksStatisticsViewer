#include "system_day_source.h"

#include <QDateTime>
#include <QGuiApplication>

#include "app/contracts/range_projection.h"

namespace ksv::qt_data {
    std::chrono::sys_days SystemDaySource::utcToday() {
        return application::epochDayOfMillis(QDateTime::currentMSecsSinceEpoch());
    }

    SystemDaySource::SystemDaySource(Clock clock, const std::chrono::milliseconds checkInterval, QObject *parent)
        : QObject(parent), m_clock(std::move(clock)), m_observed(m_clock()) {
        m_timer.setTimerType(Qt::VeryCoarseTimer);
        m_timer.setInterval(checkInterval);
        connect(&m_timer, &QTimer::timeout, this, [this] { refresh(); });
        m_timer.start();
        if (qGuiApp != nullptr) {
            connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
                    &SystemDaySource::onApplicationStateChanged);
        }
    }

    void SystemDaySource::refresh() {
        const auto now = m_clock();
        if (now == m_observed) return;
        m_observed = now;
        const auto callbacks = m_callbacks;
        for (const auto &callback: callbacks) callback(m_observed);
    }

    void SystemDaySource::onApplicationStateChanged(const Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) refresh();
    }
}
