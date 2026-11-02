#include <gtest/gtest.h>

#include <QFont>
#include <QObject>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantList>

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "capture/capture_sources.h"
#include "components/graph_canvas.h"
#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"
#include "presentation/capture/capture_vm.h"
#include "presentation/graphs/completion_history_vm.h"

#include "../benchmarks/tracking/fake_benchmark_tracking_use_case.h"
#include "capture_coordinator_support.h"

namespace ksv::ui {
    namespace {
        using namespace ksv::application;
        using namespace ksv::domain;
        using capture_coordinator_tests::drainUntil;
        using capture_coordinator_tests::GatedRender;
        using capture_coordinator_tests::RecordingRender;
        using capture_coordinator_tests::ScriptedRender;
        using capture_coordinator_tests::realRender;
        using presentation::BenchmarkTrackingViewModel;
        using presentation::CaptureLiveViewModels;
        using presentation::CaptureViewModel;
        using presentation::CompletionHistoryViewModel;
        using tests_ui::FakeBenchmarkTrackingUseCase;
        using tests_ui::makeBreakdownSnapshot;

        class FakeCompletionHistoryUseCase final : public ICompletionHistoryUseCase {
        public:
            CompletionHistory history;

            CompletionHistory get_history() override { return history; }
            void onCurrentScenarioChanged(std::function<void()>) override {}
            void onRangeChanged(std::function<void()>) override {}
            [[nodiscard]] GraphPresentation presentation() const override {
                return defaultGraphPresentation(GraphKey::ScenarioHistory);
            }
            void onPresentationChanged(std::function<void()>) override {}
            std::optional<int> selected_run_index() override { return std::nullopt; }
            void onSelectedRunChanged(std::function<void()>) override {}
            void select_run(int) override {}
        };

        class WarningCapture {
        public:
            WarningCapture() { s_count = 0; m_previous = qInstallMessageHandler(&WarningCapture::handle); }
            ~WarningCapture() { qInstallMessageHandler(m_previous); }
            WarningCapture(const WarningCapture &) = delete;
            WarningCapture &operator=(const WarningCapture &) = delete;

            [[nodiscard]] int count() const { return s_count; }

        private:
            static void handle(const QtMsgType type, const QMessageLogContext &, const QString &) {
                if (type == QtWarningMsg) ++s_count;
            }

            QtMessageHandler m_previous = nullptr;
            inline static int s_count = 0;
        };

        struct VmFixture {
            explicit VmFixture(CaptureLiveViewModels live, CaptureRenderFunction render)
                : clipboard(std::make_shared<tests_support::FakeClipboardImageWriter>()),
                  png(std::make_shared<tests_support::FakePngImageWriter>()),
                  output(std::make_shared<ImageOutputUseCase>(clipboard, png)),
                  vm(output, live, std::move(render)) {
                vm.useTheme(&theme, QFont());
            }

            QObject theme;
            std::shared_ptr<tests_support::FakeClipboardImageWriter> clipboard;
            std::shared_ptr<tests_support::FakePngImageWriter> png;
            std::shared_ptr<ImageOutputUseCase> output;
            CaptureViewModel vm;
        };

        struct HistoryFixture {
            explicit HistoryFixture(const std::string &scenarioName) {
                useCase->history = {.scenario_name = scenarioName, .full_count = 10};
                for (const int run: {4, 5, 6})
                    useCase->history.rows.push_back({.run_index = run, .start_time_ms = 1'772'000'000'000 + run, .score = 100.0 + run});
                history.refresh();
                canvas.setWidth(800);
                canvas.setHeight(400);
                canvas.setGraphVm(&history);
                canvas.setVisibleColumns(QVariantList{CompletionHistoryViewModel::Score});
            }

            std::shared_ptr<FakeCompletionHistoryUseCase> useCase = std::make_shared<FakeCompletionHistoryUseCase>();
            CompletionHistoryViewModel history{useCase};
            GraphCanvas canvas;
        };

        std::shared_ptr<FakeBenchmarkTrackingUseCase> trackableUseCase() {
            auto fake = std::make_shared<FakeBenchmarkTrackingUseCase>();
            fake->snap = makeBreakdownSnapshot();
            return fake;
        }

        std::shared_ptr<FakeBenchmarkTrackingUseCase> incompleteUseCase() {
            auto snapshot = makeBreakdownSnapshot();
            snapshot.selectedLoaded->completeness = CompletenessResult{
                Completeness::Incomplete, {BenchmarkIssue{BenchmarkIssueCode::MissingThreshold, std::monostate{}}}};
            snapshot.projection->completeness = Completeness::Incomplete;
            snapshot.projection->attainedRank.reset();
            snapshot.projection->nextTier.reset();
            snapshot.projection->averageRank.reset();
            snapshot.projection->nextTierBlockers.clear();
            auto fake = std::make_shared<FakeBenchmarkTrackingUseCase>();
            fake->snap = std::move(snapshot);
            return fake;
        }

        bool stateIs(const CaptureViewModel &vm, const char *state) {
            return vm.previewState() == QLatin1StringView(state);
        }

        CaptureRenderFunction recordingLastRender(CaptureRenderFunction inner, std::atomic<int> &percent,
                                                  std::atomic<bool> &summary, std::atomic<int> &width,
                                                  std::atomic<int> &height) {
            return [inner = std::move(inner), &percent, &summary, &width, &height](const CaptureRenderRequest &request) {
                CaptureRenderOutcome outcome = inner(request);
                if (const auto *rendered = std::get_if<CaptureRendered>(&outcome)) {
                    width = rendered->pixels.width();
                    height = rendered->pixels.height();
                    summary = request.options.includeSummary;
                    percent = request.options.percent;
                }
                return outcome;
            };
        }
    }

    TEST(CaptureViewModel, ANullCanvasIsUnavailable) {
        RecordingRender render;
        VmFixture fixture({}, render.function());

        fixture.vm.quickCopy(QStringLiteral("dailyPlaytime"), nullptr);

        EXPECT_EQ(fixture.vm.feedbackKind(), QStringLiteral("failure"));
        EXPECT_TRUE(fixture.vm.feedbackText().contains(captureTargetName(CaptureTarget::DailyPlaytime)));
    }

    TEST(CaptureViewModel, AnUnknownKeyStartsNothing) {
        RecordingRender render;
        VmFixture fixture({}, render.function());
        const WarningCapture warnings;

        fixture.vm.quickCopy(QStringLiteral("nonsense"), nullptr);

        EXPECT_GE(warnings.count(), 1);
        EXPECT_EQ(fixture.vm.feedbackKind(), QStringLiteral("none"));
        EXPECT_TRUE(render.requests().empty());
    }

    TEST(CaptureViewModel, AGraphKeyUsesItsOwnContext) {
        RecordingRender render;
        HistoryFixture history("Air Angelic");
        VmFixture fixture({.history = &history.history}, render.function());

        fixture.vm.quickCopy(QStringLiteral("scenarioHistory"), &history.canvas);

        ASSERT_TRUE(drainUntil([&] { return !render.requests().empty(); }));
        const auto &content = *render.requests().front().content;
        const auto *graph = std::get_if<GraphCaptureSnapshot>(&content);
        ASSERT_NE(graph, nullptr);
        EXPECT_EQ(graph->target, CaptureTarget::ScenarioHistory);
        EXPECT_EQ(graph->context.title, scenarioHistoryCaptureContext(history.history).title);
    }

    TEST(CaptureViewModel, TheTableKeyReadsTheTrackingPresentation) {
        RecordingRender render;
        BenchmarkTrackingViewModel tracking{trackableUseCase()};
        ASSERT_TRUE(tracking.summaryAvailable());
        VmFixture fixture({.tracking = &tracking}, render.function());

        fixture.vm.quickCopy(QStringLiteral("benchmarkTable"), nullptr);

        ASSERT_TRUE(drainUntil([&] { return !render.requests().empty(); }));
        const auto &content = *render.requests().front().content;
        const auto *table = std::get_if<TableCaptureSnapshot>(&content);
        ASSERT_NE(table, nullptr);
        EXPECT_EQ(table->benchmarkName, tracking.selectedName());
        EXPECT_TRUE(table->summary.has_value());
    }

    TEST(CaptureViewModel, TheFileNameIsSafeAndHoldsNoPath) {
        RecordingRender render;
        HistoryFixture history("Air: Angelic/Pro");
        VmFixture fixture({.history = &history.history}, render.function());

        fixture.vm.openPreview(QStringLiteral("scenarioHistory"), &history.canvas);

        const QString name = fixture.vm.suggestedFileName();
        EXPECT_TRUE(name.endsWith(QStringLiteral(".png")));
        EXPECT_FALSE(name.contains(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])"))));
    }

    TEST(CaptureViewModel, PreviewStatesReachQmlAsStrings) {
        GatedRender gate(realRender());
        gate.close();
        ScriptedRender render(gate.function());
        BenchmarkTrackingViewModel tracking{trackableUseCase()};
        VmFixture fixture({.tracking = &tracking}, render.function());
        const auto openGate = qScopeGuard([&] { gate.open(); });
        QSignalSpy stateChanges(&fixture.vm, &CaptureViewModel::previewStateChanged);

        fixture.vm.openPreview(QStringLiteral("benchmarkTable"), nullptr);

        ASSERT_TRUE(fixture.vm.previewOpen());
        EXPECT_TRUE(stateIs(fixture.vm, "rendering"));

        gate.open();
        ASSERT_TRUE(drainUntil([&] { return stateIs(fixture.vm, "ready"); }));
        EXPECT_GE(stateChanges.count(), 1);

        const CaptureRejection rejection{CaptureRejectionReason::AreaLimit, CaptureCorrection::ReduceResolution,
                                         std::nullopt};
        const auto afterReady = stateChanges.count();
        render.next(CaptureRejected{rejection, QSizeF()});
        fixture.vm.setPercent(300);
        ASSERT_TRUE(drainUntil([&] { return stateIs(fixture.vm, "invalid"); }));
        EXPECT_GT(stateChanges.count(), afterReady);

        const auto afterInvalid = stateChanges.count();
        render.next(CaptureRenderFailed{});
        fixture.vm.setPercent(100);
        ASSERT_TRUE(drainUntil([&] { return stateIs(fixture.vm, "failed"); }));
        EXPECT_GT(stateChanges.count(), afterInvalid);
    }

    TEST(CaptureViewModel, ThePreviewPropertiesFollowTheSession) {
        std::atomic<int> renderedPercent = 0;
        std::atomic<bool> renderedSummary = true;
        std::atomic<int> renderedWidth = 0;
        std::atomic<int> renderedHeight = 0;
        BenchmarkTrackingViewModel tracking{trackableUseCase()};
        VmFixture fixture({.tracking = &tracking},
                          recordingLastRender(realRender(), renderedPercent, renderedSummary, renderedWidth,
                                              renderedHeight));

        fixture.vm.openPreview(QStringLiteral("benchmarkTable"), nullptr);
        ASSERT_TRUE(drainUntil([&] { return renderedWidth > 0; }));
        static_cast<void>(drainUntil([&] { return fixture.vm.canOutput(); }));
        fixture.vm.setPercent(300);
        fixture.vm.setIncludeSummary(false);
        ASSERT_TRUE(drainUntil([&] { return renderedPercent == 300 && !renderedSummary; }));
        static_cast<void>(drainUntil([&] { return fixture.vm.outputWidth() == renderedWidth; }));

        EXPECT_EQ(fixture.vm.outputWidth(), renderedWidth);
        EXPECT_EQ(fixture.vm.outputHeight(), renderedHeight);
        EXPECT_EQ(fixture.vm.percent(), 300);
        EXPECT_FALSE(fixture.vm.includeSummary());
        EXPECT_EQ(fixture.vm.previewTargetName(), captureTargetName(CaptureTarget::BenchmarkTable));
        EXPECT_FALSE(fixture.vm.previewImage().isNull());
        EXPECT_TRUE(fixture.vm.canOutput());
        EXPECT_FALSE(fixture.vm.isGraph());
    }

    TEST(CaptureViewModel, SummaryAvailabilityFollowsTheSnapshot) {
        RecordingRender render;
        BenchmarkTrackingViewModel complete{trackableUseCase()};
        BenchmarkTrackingViewModel incomplete{incompleteUseCase()};
        VmFixture withSummary({.tracking = &complete}, render.function());
        VmFixture withoutSummary({.tracking = &incomplete}, render.function());

        withSummary.vm.openPreview(QStringLiteral("benchmarkTable"), nullptr);
        withoutSummary.vm.openPreview(QStringLiteral("benchmarkTable"), nullptr);

        EXPECT_TRUE(withSummary.vm.summaryAvailable());
        EXPECT_FALSE(withoutSummary.vm.summaryAvailable());
    }

    TEST(CaptureViewModel, DimensionErrorsReachQml) {
        HistoryFixture history("Air Angelic");
        VmFixture fixture({.history = &history.history}, realRender());

        fixture.vm.openPreview(QStringLiteral("scenarioHistory"), &history.canvas);
        static_cast<void>(drainUntil([&] { return fixture.vm.canOutput(); }));
        fixture.vm.commitWidth(QStringLiteral("abc"));

        EXPECT_EQ(fixture.vm.widthText(), QStringLiteral("abc"));
        EXPECT_FALSE(fixture.vm.widthError().isEmpty());
        EXPECT_TRUE(fixture.vm.isGraph());
        EXPECT_TRUE(fixture.vm.keepAspect());
        EXPECT_FALSE(fixture.vm.canOutput());
    }

    TEST(CaptureViewModel, ConfirmingAUrlSavesToItsLocalPath) {
        QTemporaryDir directory;
        ASSERT_TRUE(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("my capture.png"));
        BenchmarkTrackingViewModel tracking{trackableUseCase()};
        VmFixture fixture({.tracking = &tracking}, realRender());
        QSignalSpy closed(&fixture.vm, &CaptureViewModel::previewClosed);

        fixture.vm.openPreview(QStringLiteral("benchmarkTable"), nullptr);
        static_cast<void>(drainUntil([&] { return fixture.vm.canOutput(); }));
        ASSERT_TRUE(fixture.vm.beginExport());
        fixture.vm.confirmExport(QUrl::fromLocalFile(path));

        ASSERT_TRUE(drainUntil([&] { return fixture.png->writeCount() == 1; }));
        EXPECT_EQ(fixture.png->seen().front().destination, std::filesystem::path(path.toStdWString()));
        EXPECT_EQ(closed.count(), 1);
    }
}
