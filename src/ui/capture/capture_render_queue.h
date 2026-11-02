#pragma once

#include <QObject>
#include <QPointer>
#include <QThreadPool>

#include <functional>
#include <mutex>
#include <optional>

#include "capture/capture_render.h"

namespace ksv::ui {
    // One running request and one pending slot: a newer request replaces whatever is still pending.
    // Construct and submit on the GUI thread; `done` runs there, and not at all once `context` is gone.
    class CaptureRenderQueue {
    public:
        using Done = std::function<void(CaptureRenderRequest, CaptureRenderOutcome)>;

        explicit CaptureRenderQueue(CaptureRenderFunction render);
        ~CaptureRenderQueue();

        void submit(CaptureRenderRequest request, QObject *context, Done done);

    private:
        struct Job {
            CaptureRenderRequest request;
            QPointer<QObject> context;
            Done done;
        };

        void runFrom(Job job);

        CaptureRenderFunction m_render;
        std::mutex m_mutex;
        std::optional<Job> m_pending;
        bool m_running = false;
        // Results go through this object and not `context`, which a worker cannot safely touch while
        // the GUI thread may be destroying it. One still queued dies with the queue.
        QObject m_deliveryContext;
        // Last member, and drained in the destructor body: a running task uses everything above.
        QThreadPool m_pool;
    };
}
