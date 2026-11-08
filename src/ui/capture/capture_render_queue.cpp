#include "capture/capture_render_queue.h"

#include <QMetaObject>

#include <utility>

namespace ksv::ui {
    CaptureRenderQueue::CaptureRenderQueue(CaptureRenderFunction render) : m_render(std::move(render)) {
        m_pool.setMaxThreadCount(1);
    }

    CaptureRenderQueue::~CaptureRenderQueue() {
        {
            const std::lock_guard lock(m_mutex);
            m_pending.reset();
            if (m_running) m_runningCancelled->store(true);
        }
        m_pool.waitForDone();
    }

    void CaptureRenderQueue::submit(CaptureRenderRequest request, QObject *context, Done done) {
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        request.cancelled = cancelled;
        Job job{std::move(request), context, std::move(done), std::move(cancelled)};
        {
            const std::lock_guard lock(m_mutex);
            if (m_running) {
                m_pending = std::move(job);
                return;
            }
            m_running = true;
            m_runningSession = job.request.session;
            m_runningCancelled = job.cancelled;
        }
        m_pool.start([this, job = std::move(job)]() mutable { runFrom(std::move(job)); });
    }

    void CaptureRenderQueue::cancel(const quint64 session) {
        const std::lock_guard lock(m_mutex);
        if (m_pending && m_pending->request.session == session) m_pending.reset();
        if (m_running && m_runningSession == session) m_runningCancelled->store(true);
    }

    void CaptureRenderQueue::runFrom(Job job) {
        while (true) {
            CaptureRenderOutcome outcome = CaptureRenderFailed{};
            try {
                outcome = m_render(job.request);
            } catch (...) {
            }
            if (!job.cancelled->load()) {
                QMetaObject::invokeMethod(
                        &m_deliveryContext,
                        [job = std::move(job), outcome = std::move(outcome)]() mutable {
                            if (job.context) job.done(std::move(job.request), std::move(outcome));
                        },
                        Qt::QueuedConnection);
            }

            const std::lock_guard lock(m_mutex);
            if (!m_pending) {
                m_running = false;
                m_runningCancelled.reset();
                return;
            }
            job = std::move(*m_pending);
            m_pending.reset();
            m_runningSession = job.request.session;
            m_runningCancelled = job.cancelled;
        }
    }
}
