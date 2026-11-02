#include <gtest/gtest.h>

#include <QObject>
#include <QScopeGuard>
#include <QTest>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "capture/capture_render.h"
#include "capture/capture_render_queue.h"

namespace ksv::ui {
    namespace {
        constexpr int kDeadlineMs = 5000;
        constexpr int kQuietWindowMs = 100;

        // Destruction joins a running render, so a failed assertion must open the gate before the queue goes.
        class Gate {
        public:
            void close() {
                const std::lock_guard lock(m_mutex);
                m_open = false;
            }

            void open() {
                {
                    const std::lock_guard lock(m_mutex);
                    m_open = true;
                }
                m_changed.notify_all();
            }

            void wait() {
                std::unique_lock lock(m_mutex);
                m_changed.wait(lock, [this] { return m_open; });
            }

        private:
            std::mutex m_mutex;
            std::condition_variable m_changed;
            bool m_open = true;
        };

        struct Recorder {
            Gate gate;
            std::mutex mutex;
            std::vector<quint64> ran;
            std::thread::id renderThread;
            std::atomic<int> started = 0;

            CaptureRenderFunction function() {
                return [this](const CaptureRenderRequest &request) -> CaptureRenderOutcome {
                    {
                        const std::lock_guard lock(mutex);
                        ran.push_back(request.generation);
                        renderThread = std::this_thread::get_id();
                    }
                    ++started;
                    gate.wait();
                    return CaptureRenderFailed{};
                };
            }

            std::vector<quint64> ranGenerations() {
                const std::lock_guard lock(mutex);
                return ran;
            }

            [[nodiscard]] bool waitForStart(const int count) {
                return QTest::qWaitFor([&] { return started.load() >= count; }, kDeadlineMs);
            }
        };

        CaptureRenderRequest requestWith(const quint64 generation) {
            return {1, generation, nullptr, {}};
        }
    }

    TEST(CaptureRenderQueue, RunsOffTheGuiThreadAndDeliversOnIt) {
        Recorder recorder;
        CaptureRenderQueue queue(recorder.function());
        QObject context;
        std::vector<quint64> delivered;
        std::thread::id deliveryThread;

        queue.submit(requestWith(7), &context, [&](const CaptureRenderRequest &request, CaptureRenderOutcome) {
            delivered.push_back(request.generation);
            deliveryThread = std::this_thread::get_id();
        });

        ASSERT_TRUE(QTest::qWaitFor([&] { return !delivered.empty(); }, kDeadlineMs));
        EXPECT_EQ(delivered, std::vector<quint64>{7});
        EXPECT_EQ(deliveryThread, std::this_thread::get_id());
        EXPECT_NE(recorder.renderThread, std::this_thread::get_id());
    }

    TEST(CaptureRenderQueue, ANewerRequestReplacesThePendingOne) {
        Recorder recorder;
        recorder.gate.close();
        CaptureRenderQueue queue(recorder.function());
        const auto openOnFailure = qScopeGuard([&] { recorder.gate.open(); });
        QObject context;
        std::vector<quint64> delivered;
        const auto done = [&](const CaptureRenderRequest &request, CaptureRenderOutcome) {
            delivered.push_back(request.generation);
        };

        queue.submit(requestWith(1), &context, done);
        ASSERT_TRUE(recorder.waitForStart(1));
        queue.submit(requestWith(2), &context, done);
        queue.submit(requestWith(3), &context, done);
        recorder.gate.open();

        const bool drained = QTest::qWaitFor([&] { return delivered.size() >= 2; }, kDeadlineMs);
        EXPECT_FALSE(QTest::qWaitFor([&] { return delivered.size() > 2; }, kQuietWindowMs));
        ASSERT_TRUE(drained);
        EXPECT_EQ(delivered, (std::vector<quint64>{1, 3}));
        EXPECT_EQ(recorder.ranGenerations(), (std::vector<quint64>{1, 3}));
    }

    TEST(CaptureRenderQueue, ADestroyedContextDropsTheResult) {
        Recorder recorder;
        recorder.gate.close();
        CaptureRenderQueue queue(recorder.function());
        bool droppedDelivered = false;
        {
            QObject context;
            queue.submit(requestWith(1), &context,
                         [&](const CaptureRenderRequest &, CaptureRenderOutcome) { droppedDelivered = true; });
            recorder.waitForStart(1);
        }
        recorder.gate.open();

        QObject next;
        bool followUpDelivered = false;
        queue.submit(requestWith(2), &next,
                     [&](const CaptureRenderRequest &, CaptureRenderOutcome) { followUpDelivered = true; });

        EXPECT_TRUE(QTest::qWaitFor([&] { return followUpDelivered; }, kDeadlineMs));
        EXPECT_FALSE(droppedDelivered);
    }

    // The app closes with a render running: its result has nowhere to go, and anything still pending must not start.
    TEST(CaptureRenderQueue, DestructionDiscardsPendingWorkAndJoins) {
        Recorder recorder;
        recorder.gate.close();
        auto queue = std::make_unique<CaptureRenderQueue>(recorder.function());
        const auto openOnFailure = qScopeGuard([&] { recorder.gate.open(); });
        QObject context;
        const auto ignored = [](const CaptureRenderRequest &, CaptureRenderOutcome) {};

        queue->submit(requestWith(1), &context, ignored);
        ASSERT_TRUE(recorder.waitForStart(1));
        queue->submit(requestWith(2), &context, ignored);
        std::thread opener([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            recorder.gate.open();
        });

        queue.reset();
        opener.join();

        EXPECT_EQ(recorder.ranGenerations(), std::vector<quint64>{1});
    }
}
