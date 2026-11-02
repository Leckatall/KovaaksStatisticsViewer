#pragma once

#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QPointF>
#include <QSemaphore>
#include <QString>
#include <QStringList>
#include <QTest>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "app/usecases/image_output_use_case.h"
#include "capture/capture_coordinator.h"
#include "capture/capture_feedback.h"
#include "capture/capture_limits.h"
#include "capture/capture_render.h"
#include "capture/capture_snapshot.h"
#include "capture/graph_capture_layout.h"
#include "components/series_painter.h"
#include "fake_image_output.h"
#include "table_capture_fixtures.h"

namespace ksv::ui::capture_coordinator_tests {
    inline constexpr int kFeedbackExpiryMs = 4000;
    inline constexpr int kDeadlineMs = 5000;
    inline constexpr int kQuietWindowMs = 100;

    template <typename Predicate>
    bool drainUntil(Predicate predicate) {
        return QTest::qWaitFor(predicate, kDeadlineMs);
    }

    inline CaptureRenderFunction realRender() {
        return makeCaptureRenderFunction(provisionalCaptureLimits(), provisionalCaptureComposition());
    }

    // Holds a render on its worker. Destroying a coordinator joins that worker, so a test must open
    // the gate from a qScopeGuard declared after the Harness, or a failed assertion hangs.
    class GatedRender {
    public:
        explicit GatedRender(CaptureRenderFunction inner) : m_inner(std::move(inner)) {
        }

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

        CaptureRenderFunction function() {
            return [this](const CaptureRenderRequest &request) {
                {
                    std::unique_lock lock(m_mutex);
                    m_changed.wait(lock, [this] { return m_open; });
                }
                return m_inner(request);
            };
        }

    private:
        CaptureRenderFunction m_inner;
        std::mutex m_mutex;
        std::condition_variable m_changed;
        bool m_open = true;
    };

    // Each render takes one permit, so a test can finish queued renders one at a time. Destroying a
    // coordinator joins its worker, so release plenty from a qScopeGuard declared after the Harness.
    class SteppedRender {
    public:
        explicit SteppedRender(CaptureRenderFunction inner) : m_inner(std::move(inner)) {
        }

        void release(int renders = 1) {
            m_permits.release(renders);
        }

        [[nodiscard]] int started() const {
            return m_started;
        }

        [[nodiscard]] int finished() const {
            return m_finished;
        }

        CaptureRenderFunction function() {
            return [this](const CaptureRenderRequest &request) {
                ++m_started;
                m_permits.acquire();
                CaptureRenderOutcome outcome = m_inner(request);
                ++m_finished;
                return outcome;
            };
        }

    private:
        CaptureRenderFunction m_inner;
        QSemaphore m_permits;
        std::atomic<int> m_started = 0;
        std::atomic<int> m_finished = 0;
    };

    // Must outlive the Harness rendering through it.
    class TrackedRender {
    public:
        explicit TrackedRender(CaptureRenderFunction inner) : m_inner(std::move(inner)) {
        }

        [[nodiscard]] int renders() const {
            return m_renders;
        }

        [[nodiscard]] const std::uint32_t *latestPixels() const {
            return m_latestPixels;
        }

        CaptureRenderFunction function() {
            return [this](const CaptureRenderRequest &request) {
                CaptureRenderOutcome outcome = m_inner(request);
                if (const auto *rendered = std::get_if<CaptureRendered>(&outcome)) {
                    m_latestPixels = rendered->raster.pixels();
                }
                ++m_renders;
                return outcome;
            };
        }

    private:
        CaptureRenderFunction m_inner;
        std::atomic<int> m_renders = 0;
        std::atomic<const std::uint32_t *> m_latestPixels = nullptr;
    };

    // Must outlive the Harness rendering through it.
    class ScriptedRender {
    public:
        explicit ScriptedRender(CaptureRenderFunction inner) : m_inner(std::move(inner)) {
        }

        void next(CaptureRenderOutcome outcome) {
            const std::lock_guard lock(m_mutex);
            m_script.push_back(std::move(outcome));
        }

        [[nodiscard]] std::vector<CaptureRenderRequest> requests() {
            const std::lock_guard lock(m_mutex);
            return m_requests;
        }

        CaptureRenderFunction function() {
            return [this](const CaptureRenderRequest &request) {
                std::optional<CaptureRenderOutcome> scripted;
                {
                    const std::lock_guard lock(m_mutex);
                    m_requests.push_back(request);
                    if (!m_script.empty()) {
                        scripted = std::move(m_script.front());
                        m_script.pop_front();
                    }
                }
                return scripted ? std::move(*scripted) : m_inner(request);
            };
        }

    private:
        CaptureRenderFunction m_inner;
        std::mutex m_mutex;
        std::deque<CaptureRenderOutcome> m_script;
        std::vector<CaptureRenderRequest> m_requests;
    };

    // Must outlive the Harness rendering through it.
    class RecordingRender {
    public:
        CaptureRenderFunction function() {
            return [this](const CaptureRenderRequest &request) -> CaptureRenderOutcome {
                const std::lock_guard lock(m_mutex);
                m_requests.push_back(request);
                return CaptureRenderFailed{};
            };
        }

        [[nodiscard]] std::vector<CaptureRenderRequest> requests() {
            const std::lock_guard lock(m_mutex);
            return m_requests;
        }

    private:
        std::mutex m_mutex;
        std::vector<CaptureRenderRequest> m_requests;
    };

    inline CaptureSource countedSource(int &calls, std::optional<CaptureContent> content) {
        return [&calls, content = std::move(content)] {
            ++calls;
            return content;
        };
    }

    inline application::RasterImage smallRaster() {
        auto buffer = application::RasterBuffer::allocate(4, 3);
        EXPECT_TRUE(buffer.has_value());
        return std::move(*buffer).freeze();
    }

    inline GraphCaptureSnapshot graphSnapshot() {
        GraphSceneAxis xAxis;
        xAxis.min = 0.0;
        xAxis.max = 10.0;
        xAxis.ticks = {0.0, 10.0};
        xAxis.labels = {QStringLiteral("0"), QStringLiteral("10")};
        xAxis.minLabel = QStringLiteral("0");
        xAxis.maxLabel = QStringLiteral("10");

        GraphSceneAxis yAxis = xAxis;
        yAxis.max = 100.0;
        yAxis.ticks = {0.0, 100.0};
        yAxis.labels = {QStringLiteral("0"), QStringLiteral("100")};
        yAxis.maxLabel = QStringLiteral("100");

        GraphSceneSeries line;
        line.column = 1;
        line.id = QStringLiteral("series-1");
        line.name = QStringLiteral("Damage");
        line.color = Qt::red;
        line.lineWidth = 2.0;
        line.observations = {QPointF(0.0, 0.0), QPointF(5.0, 100.0), QPointF(10.0, 0.0)};
        line.xAxis = xAxis;
        line.yAxis = yAxis;

        GraphCaptureSnapshot snapshot;
        snapshot.target = CaptureTarget::Run;
        snapshot.context.title = QStringLiteral("Pasu Track Invincible");
        snapshot.context.graphType = QStringLiteral("Run graph");
        snapshot.context.rangeText = QStringLiteral("Elapsed 0:00 to 1:00");
        snapshot.context.xTitle = QStringLiteral("Elapsed seconds");
        snapshot.appearance.window = QColor(16, 24, 32);
        snapshot.appearance.text = QColor(240, 240, 240);
        snapshot.appearance.placeholderText = QColor(160, 160, 160);
        const QFont app = QGuiApplication::font();
        snapshot.appearance.font.families = app.families().isEmpty() ? QStringList{app.family()} : app.families();
        snapshot.appearance.font.pixelSize = 14;
        snapshot.scene.series.append(line);
        snapshot.scene.xAxis = xAxis;
        snapshot.scene.labelledYAxis = yAxis;
        snapshot.scene.labelledColumn = 1;
        snapshot.scene.labelledColumns = {1};
        snapshot.scene.labelledYAxisTitle = QStringLiteral("Damage done");
        snapshot.scene.connection = SeriesPainter::Connection::Straight;
        snapshot.scene.background = snapshot.appearance.window;
        return snapshot;
    }

    inline TableCaptureSnapshot tableSnapshotWithSummary() {
        TableCaptureSnapshot snapshot = largeTableFixture();
        snapshot.summary = tableFixtureSummary(snapshot.grid.tiers);
        return snapshot;
    }

    struct Harness {
        explicit Harness(CaptureRenderFunction render)
            : clipboard(std::make_shared<tests_support::FakeClipboardImageWriter>()),
              png(std::make_shared<tests_support::FakePngImageWriter>()),
              output(std::make_shared<application::ImageOutputUseCase>(clipboard, png)),
              feedback(kFeedbackExpiryMs),
              coordinator(output, std::move(render), &feedback) {
        }

        std::shared_ptr<tests_support::FakeClipboardImageWriter> clipboard;
        std::shared_ptr<tests_support::FakePngImageWriter> png;
        std::shared_ptr<application::ImageOutputUseCase> output;
        CaptureFeedback feedback;
        CaptureCoordinator coordinator;
    };
}
