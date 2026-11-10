#include <gtest/gtest.h>

#include <QScopeGuard>
#include <QSignalSpy>
#include <QSize>
#include <QSizeF>
#include <QTest>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "capture_coordinator_support.h"

namespace ksv::ui {
    namespace {
        using namespace capture_coordinator_tests;

        CaptureRenderOutcome failRender(const CaptureRenderRequest &) {
            return CaptureRenderFailed{};
        }

        using PreviewState = CaptureCoordinator::PreviewState;

        bool isReadySoon(const Harness &harness) {
            return drainUntil([&] { return harness.coordinator.previewState() == PreviewState::Ready; });
        }

        void drainUntilSettled(const Harness &harness) {
            drainUntil([&] { return harness.coordinator.previewState() != PreviewState::Rendering; });
        }

        QSize scaled(QSizeF reference, double factor) {
            return QSize(qRound(reference.width() * factor), qRound(reference.height() * factor));
        }

        bool saysSaving(const CaptureFeedback &feedback) {
            return feedback.text().contains(QStringLiteral("saving"), Qt::CaseInsensitive);
        }

        TEST(CaptureCoordinator, StartsWithNoPreview) {
            Harness harness(failRender);

            EXPECT_FALSE(harness.coordinator.previewOpen());
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::None);
        }

        TEST(CaptureCoordinator, QuickCopyPublishesTheDoubledReference) {
            const GraphCaptureSnapshot snapshot = graphSnapshot();
            const QSizeF reference = graphReferenceSize(snapshot, standardCaptureComposition());
            Harness harness(realRender());
            int sourceCalls = 0;

            harness.coordinator.quickCopy(CaptureTarget::Run, countedSource(sourceCalls, snapshot));
            drainUntil([&] { return harness.feedback.kind() != CaptureFeedback::Kind::None; });

            ASSERT_EQ(harness.clipboard->publishCount(), 1u);
            EXPECT_EQ(sourceCalls, 1);
            EXPECT_EQ(harness.clipboard->seen().front().width, qRound(reference.width() * 2.0));
            EXPECT_EQ(harness.clipboard->seen().front().height, qRound(reference.height() * 2.0));
            EXPECT_FALSE(harness.coordinator.previewOpen());
        }

        TEST(CaptureCoordinator, SuccessWaitsForTheClipboardResult) {
            GatedRender render(realRender());
            render.close();
            Harness harness(render.function());
            const auto openGate = qScopeGuard([&] { render.open(); });
            int sourceCalls = 0;

            harness.coordinator.quickCopy(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            QTest::qWait(kQuietWindowMs);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::None);

            render.open();
            drainUntil([&] { return harness.feedback.kind() != CaptureFeedback::Kind::None; });

            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Success);
            EXPECT_TRUE(harness.feedback.text().contains(captureTargetName(CaptureTarget::Run)));
        }

        TEST(CaptureCoordinator, TableQuickCopyIncludesTheSummary) {
            RecordingRender render;
            Harness harness(render.function());
            int sourceCalls = 0;

            harness.coordinator.quickCopy(CaptureTarget::BenchmarkTable,
                                          countedSource(sourceCalls, tableSnapshotWithSummary()));

            ASSERT_TRUE(drainUntil([&] { return !render.requests().empty(); }));
            EXPECT_EQ(render.requests().front().options.percent, 200);
            EXPECT_TRUE(render.requests().front().options.includeSummary);
        }

        // The use case has no handler removal, so its completion can reach a dead coordinator. The
        // clipboard hook queues the destruction ahead of the completion copyImage posts after publish().
        TEST(CaptureCoordinator, ALateCompletionAfterDestructionIsIgnored) {
            auto clipboard = std::make_shared<tests_support::FakeClipboardImageWriter>();
            auto output = std::make_shared<application::ImageOutputUseCase>(
                    clipboard, std::make_shared<tests_support::FakePngImageWriter>());
            CaptureFeedback feedback(kFeedbackExpiryMs);
            auto coordinator = std::make_unique<CaptureCoordinator>(output, realRender(), &feedback);
            clipboard->setHook([&] {
                QMetaObject::invokeMethod(&feedback, [&] { coordinator.reset(); }, Qt::QueuedConnection);
            });
            bool delivered = false;
            output->onCompleted([&](const application::ImageOutputResult &) {
                delivered = true;
                EXPECT_EQ(coordinator, nullptr);
            });
            int sourceCalls = 0;

            coordinator->quickCopy(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));

            EXPECT_TRUE(drainUntil([&] { return delivered; }));
            EXPECT_EQ(feedback.kind(), CaptureFeedback::Kind::None);
        }

        TEST(CaptureCoordinator, AnotherCallersOutputIsNotReported) {
            Harness harness(failRender);

            ASSERT_TRUE(harness.output->copyImage(smallRaster(), "someone else").accepted);

            EXPECT_TRUE(drainUntil([&] { return !harness.output->activeOutput().has_value(); }));
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::None);
        }

        TEST(CaptureCoordinator, ARepeatOfTheSameTargetIsIgnored) {
            GatedRender render(realRender());
            render.close();
            Harness harness(render.function());
            const auto openGate = qScopeGuard([&] { render.open(); });
            int sourceCalls = 0;
            const CaptureSource source = countedSource(sourceCalls, graphSnapshot());

            harness.coordinator.quickCopy(CaptureTarget::Run, source);
            harness.coordinator.quickCopy(CaptureTarget::Run, source);

            EXPECT_EQ(sourceCalls, 1);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::None);

            render.open();
            drainUntil([&] { return harness.feedback.kind() != CaptureFeedback::Kind::None; });
            EXPECT_EQ(harness.clipboard->publishCount(), 1u);
        }

        TEST(CaptureCoordinator, AnotherTargetIsRejectedWhilePreparing) {
            GatedRender render(realRender());
            render.close();
            Harness harness(render.function());
            const auto openGate = qScopeGuard([&] { render.open(); });
            int runCalls = 0;
            int playtimeCalls = 0;

            harness.coordinator.quickCopy(CaptureTarget::Run, countedSource(runCalls, graphSnapshot()));
            harness.coordinator.quickCopy(CaptureTarget::DailyPlaytime, countedSource(playtimeCalls, graphSnapshot()));

            EXPECT_EQ(playtimeCalls, 0);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);
        }

        // Nothing reaches the clipboard until the first copy finishes rendering.
        TEST(CaptureCoordinator, ARejectionWhileACopyRendersDoesNotSayCopying) {
            GatedRender render(realRender());
            render.close();
            Harness harness(render.function());
            const auto openGate = qScopeGuard([&] { render.open(); });
            int runCalls = 0;
            int playtimeCalls = 0;

            harness.coordinator.quickCopy(CaptureTarget::Run, countedSource(runCalls, graphSnapshot()));
            harness.coordinator.quickCopy(CaptureTarget::DailyPlaytime, countedSource(playtimeCalls, graphSnapshot()));

            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);
            EXPECT_FALSE(harness.feedback.text().contains(QStringLiteral("copied"), Qt::CaseInsensitive));
            EXPECT_FALSE(saysSaving(harness.feedback));
        }

        // A copy is admitted and published in one call but delivers its result a turn later, so the
        // use case still reports it as active, and telling the user an image is saving would be false.
        TEST(CaptureCoordinator, ARejectionAfterACopyDoesNotSaySaving) {
            Harness harness(realRender());
            int sourceCalls = 0;
            ASSERT_TRUE(harness.output->copyImage(smallRaster(), "someone else").accepted);

            harness.coordinator.quickCopy(CaptureTarget::DailyPlaytime, countedSource(sourceCalls, graphSnapshot()));

            EXPECT_EQ(sourceCalls, 0);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);
            EXPECT_FALSE(saysSaving(harness.feedback));
        }

        TEST(CaptureCoordinator, ARejectionDuringASaveSaysSaving) {
            Harness harness(realRender());
            const auto openGate = qScopeGuard([&] { harness.png->openGate(); });
            harness.png->closeGate();
            int sourceCalls = 0;
            ASSERT_TRUE(harness.output->savePng(smallRaster(), "capture.png", "someone else").accepted);

            harness.coordinator.quickCopy(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));

            EXPECT_EQ(sourceCalls, 0);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);
            EXPECT_TRUE(saysSaving(harness.feedback));
        }

        TEST(CaptureCoordinator, NoContentExplainsItself) {
            Harness harness(failRender);
            int emptyCalls = 0;
            int laterCalls = 0;

            harness.coordinator.quickCopy(CaptureTarget::Run, countedSource(emptyCalls, std::nullopt));

            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Failure);
            EXPECT_TRUE(harness.feedback.text().contains(captureTargetName(CaptureTarget::Run)));
            EXPECT_EQ(harness.clipboard->publishCount(), 0u);

            harness.coordinator.quickCopy(CaptureTarget::Run, countedSource(laterCalls, graphSnapshot()));
            EXPECT_EQ(laterCalls, 1);
        }

        TEST(CaptureCoordinator, ARejectedQuickCopyReportsTheCorrection) {
            const CaptureRejection rejection{CaptureRejectionReason::EdgeLimit,
                                             CaptureCorrection::ReduceDimensionsOrResolution, std::nullopt};
            Harness harness([rejection](const CaptureRenderRequest &) -> CaptureRenderOutcome {
                return CaptureRejected{rejection, QSizeF()};
            });
            int sourceCalls = 0;

            harness.coordinator.quickCopy(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            drainUntil([&] { return harness.feedback.kind() != CaptureFeedback::Kind::None; });

            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Failure);
            EXPECT_TRUE(harness.feedback.text().contains(captureRejectionText(rejection)));
            EXPECT_EQ(harness.clipboard->publishCount(), 0u);
        }

        TEST(CaptureCoordinator, ARenderFailureReopensAdmission) {
            Harness harness(failRender);
            int sourceCalls = 0;
            const CaptureSource source = countedSource(sourceCalls, graphSnapshot());

            harness.coordinator.quickCopy(CaptureTarget::Run, source);
            drainUntil([&] { return harness.feedback.kind() != CaptureFeedback::Kind::None; });
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Failure);

            harness.coordinator.quickCopy(CaptureTarget::Run, source);
            EXPECT_EQ(sourceCalls, 2);
        }

        TEST(CaptureCoordinator, AnUnconfirmedClipboardIsAFailure) {
            Harness harness(realRender());
            harness.clipboard->setStatus(application::ImageOutputStatus::ClipboardNotConfirmed);
            int sourceCalls = 0;

            harness.coordinator.quickCopy(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            drainUntil([&] { return harness.feedback.kind() != CaptureFeedback::Kind::None; });

            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Failure);
            EXPECT_TRUE(harness.feedback.text().contains(captureTargetName(CaptureTarget::Run)));
            EXPECT_TRUE(harness.feedback.text().contains(QStringLiteral("copy"), Qt::CaseInsensitive));
        }

        TEST(CaptureCoordinator, OpeningRendersAtDefaults) {
            const GraphCaptureSnapshot snapshot = graphSnapshot();
            const QSizeF reference = graphReferenceSize(snapshot, standardCaptureComposition());
            Harness harness(realRender());
            int sourceCalls = 0;

            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, snapshot));

            ASSERT_TRUE(harness.coordinator.previewOpen());
            EXPECT_EQ(harness.coordinator.previewState(), PreviewState::Rendering);
            EXPECT_FALSE(harness.coordinator.canOutput());

            ASSERT_TRUE(isReadySoon(harness));
            EXPECT_FALSE(harness.coordinator.previewImage().isNull());
            EXPECT_EQ(harness.coordinator.outputSize(),
                      QSize(qRound(reference.width() * 2.0), qRound(reference.height() * 2.0)));
            EXPECT_TRUE(harness.coordinator.canOutput());
            EXPECT_EQ(harness.clipboard->publishCount(), 0u);
        }

        TEST(CaptureCoordinator, UnavailableContentOpensNoPreview) {
            Harness harness(failRender);
            int sourceCalls = 0;

            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, std::nullopt));

            EXPECT_FALSE(harness.coordinator.previewOpen());
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Failure);
            EXPECT_TRUE(harness.feedback.text().contains(captureTargetName(CaptureTarget::Run)));
        }

        TEST(CaptureCoordinator, PreviewIsRejectedWhileAQuickCopyPrepares) {
            GatedRender render(realRender());
            render.close();
            Harness harness(render.function());
            const auto openGate = qScopeGuard([&] { render.open(); });
            int runCalls = 0;
            int playtimeCalls = 0;

            harness.coordinator.quickCopy(CaptureTarget::Run, countedSource(runCalls, graphSnapshot()));
            harness.coordinator.openPreview(CaptureTarget::DailyPlaytime, countedSource(playtimeCalls, graphSnapshot()));

            EXPECT_FALSE(harness.coordinator.previewOpen());
            EXPECT_EQ(playtimeCalls, 0);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);
        }

        TEST(CaptureCoordinator, ClosingDiscardsALateRender) {
            SteppedRender render(realRender());
            Harness harness(render.function());
            const auto releaseAll = qScopeGuard([&] { render.release(8); });
            QSignalSpy closed(&harness.coordinator, &CaptureCoordinator::previewClosed);
            int firstCalls = 0;
            int secondCalls = 0;

            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(firstCalls, graphSnapshot()));
            ASSERT_TRUE(harness.coordinator.previewOpen());
            harness.coordinator.closePreview();
            harness.coordinator.openPreview(CaptureTarget::DailyPlaytime, countedSource(secondCalls, graphSnapshot()));

            render.release();
            ASSERT_TRUE(drainUntil([&] { return render.finished() == 1; }));
            QTest::qWait(kQuietWindowMs);
            EXPECT_TRUE(harness.coordinator.previewImage().isNull());
            EXPECT_FALSE(harness.coordinator.canOutput());
            EXPECT_EQ(harness.coordinator.previewState(), PreviewState::Rendering);

            render.release();
            ASSERT_TRUE(isReadySoon(harness));
            EXPECT_TRUE(harness.coordinator.canOutput());
            EXPECT_EQ(closed.count(), 1);
        }

        // A preview render paints until cancelled, far beyond any deadline, as a dense graph can.
        // Quick copies, which carry no session, render normally.
        class EndlessPreviewRender {
        public:
            [[nodiscard]] std::vector<quint64> previewGenerations() {
                const std::lock_guard lock(m_mutex);
                return m_previewGenerations;
            }

            [[nodiscard]] int previewsStarted() {
                const std::lock_guard lock(m_mutex);
                return static_cast<int>(m_previewGenerations.size());
            }

            CaptureRenderFunction function() {
                return [this, inner = realRender()](const CaptureRenderRequest &request) -> CaptureRenderOutcome {
                    if (request.session == 0) return inner(request);
                    {
                        const std::lock_guard lock(m_mutex);
                        m_previewGenerations.push_back(request.generation);
                    }
                    const auto giveUp = std::chrono::steady_clock::now() + std::chrono::seconds(30);
                    while (!request.isCancelled() && std::chrono::steady_clock::now() < giveUp) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    return CaptureRenderFailed{};
                };
            }

        private:
            std::mutex m_mutex;
            std::vector<quint64> m_previewGenerations;
        };

        TEST(CaptureCoordinator, ClosingStopsTheRunningRenderAndDropsThePendingOne) {
            EndlessPreviewRender render;
            Harness harness(render.function());
            int previewCalls = 0;
            int copyCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(previewCalls, graphSnapshot()));
            ASSERT_TRUE(drainUntil([&] { return render.previewsStarted() == 1; }));
            harness.coordinator.setPercent(300);

            harness.coordinator.closePreview();
            harness.coordinator.quickCopy(CaptureTarget::Run, countedSource(copyCalls, graphSnapshot()));

            EXPECT_TRUE(drainUntil([&] { return harness.feedback.kind() == CaptureFeedback::Kind::Success; }));
            EXPECT_EQ(render.previewGenerations(), std::vector<quint64>{1});
        }

        TEST(CaptureCoordinator, DestructionStopsARunningRenderPromptly) {
            EndlessPreviewRender render;
            auto harness = std::make_unique<Harness>(render.function());
            int calls = 0;
            harness->coordinator.openPreview(CaptureTarget::Run, countedSource(calls, graphSnapshot()));
            ASSERT_TRUE(drainUntil([&] { return render.previewsStarted() == 1; }));

            const auto before = std::chrono::steady_clock::now();
            harness.reset();

            EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::milliseconds(kDeadlineMs));
        }

        TEST(CaptureCoordinator, AResolutionOutsideTheOfferedRangeIsClamped) {
            RecordingRender render;
            Harness harness(render.function());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));

            harness.coordinator.setPercent(kMinimumCapturePercent - 40);
            ASSERT_TRUE(drainUntil([&] { return render.requests().size() >= 2; }));
            EXPECT_EQ(render.requests().back().options.percent, kMinimumCapturePercent);
            EXPECT_EQ(harness.coordinator.percent(), kMinimumCapturePercent);

            harness.coordinator.setPercent(kCaptureResolutionMarks.back() * 2);
            ASSERT_TRUE(drainUntil([&] { return render.requests().size() >= 3; }));
            EXPECT_EQ(render.requests().back().options.percent, kCaptureResolutionMarks.back());
        }

        TEST(CaptureCoordinator, ReopeningTakesANewSnapshot) {
            Harness harness(realRender());
            int sourceCalls = 0;
            const CaptureSource source = countedSource(sourceCalls, graphSnapshot());

            harness.coordinator.openPreview(CaptureTarget::Run, source);
            ASSERT_EQ(sourceCalls, 1);
            ASSERT_TRUE(isReadySoon(harness));
            harness.coordinator.closePreview();

            harness.coordinator.openPreview(CaptureTarget::Run, source);

            EXPECT_EQ(sourceCalls, 2);
            EXPECT_TRUE(harness.coordinator.previewImage().isNull());
            ASSERT_TRUE(isReadySoon(harness));
            EXPECT_FALSE(harness.coordinator.previewImage().isNull());
        }

        TEST(CaptureCoordinator, QuickCopyIsRejectedWhileAPreviewIsOpen) {
            GatedRender render(realRender());
            render.close();
            Harness harness(render.function());
            const auto openGate = qScopeGuard([&] { render.open(); });
            int runCalls = 0;
            int playtimeCalls = 0;

            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(runCalls, graphSnapshot()));
            harness.coordinator.quickCopy(CaptureTarget::DailyPlaytime, countedSource(playtimeCalls, graphSnapshot()));

            EXPECT_EQ(playtimeCalls, 0);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);

            render.open();
            EXPECT_TRUE(isReadySoon(harness));
        }

        TEST(CaptureCoordinator, AChangeHoldsTheOldImageButNotTheOutput) {
            const GraphCaptureSnapshot snapshot = graphSnapshot();
            const QSizeF reference = graphReferenceSize(snapshot, standardCaptureComposition());
            GatedRender render(realRender());
            Harness harness(render.function());
            const auto openGate = qScopeGuard([&] { render.open(); });
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, snapshot));
            ASSERT_TRUE(isReadySoon(harness));

            render.close();
            harness.coordinator.setPercent(300);

            EXPECT_EQ(harness.coordinator.previewState(), PreviewState::Rendering);
            EXPECT_FALSE(harness.coordinator.previewImage().isNull());
            EXPECT_FALSE(harness.coordinator.canOutput());
            harness.coordinator.copyPreview();
            EXPECT_EQ(harness.clipboard->publishCount(), 0u);

            render.open();
            ASSERT_TRUE(isReadySoon(harness));
            EXPECT_EQ(harness.coordinator.outputSize(), scaled(reference, 3.0));
        }

        TEST(CaptureCoordinator, OnlyTheLatestChoiceInstalls) {
            const GraphCaptureSnapshot snapshot = graphSnapshot();
            const QSizeF reference = graphReferenceSize(snapshot, standardCaptureComposition());
            GatedRender render(realRender());
            Harness harness(render.function());
            const auto openGate = qScopeGuard([&] { render.open(); });
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, snapshot));
            ASSERT_TRUE(isReadySoon(harness));

            std::vector<QSize> installed;
            QObject::connect(&harness.coordinator, &CaptureCoordinator::previewChanged, &harness.coordinator, [&] {
                installed.push_back(harness.coordinator.outputSize());
            });
            render.close();
            harness.coordinator.setPercent(300);
            harness.coordinator.setPercent(400);
            harness.coordinator.setPercent(500);
            render.open();

            ASSERT_TRUE(isReadySoon(harness));
            EXPECT_EQ(harness.coordinator.outputSize(), scaled(reference, 5.0));
            EXPECT_EQ(std::count(installed.begin(), installed.end(), scaled(reference, 3.0)), 0);
            EXPECT_EQ(std::count(installed.begin(), installed.end(), scaled(reference, 4.0)), 0);
        }

        TEST(CaptureCoordinator, TheSummaryOptionChangesTheOutputHeight) {
            Harness harness(realRender());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::BenchmarkTable,
                                            countedSource(sourceCalls, tableSnapshotWithSummary()));
            ASSERT_TRUE(isReadySoon(harness));
            const QSize withSummary = harness.coordinator.outputSize();

            harness.coordinator.setIncludeSummary(false);
            ASSERT_TRUE(isReadySoon(harness));

            EXPECT_LT(harness.coordinator.outputSize().height(), withSummary.height());
            EXPECT_EQ(harness.coordinator.outputSize().width(), withSummary.width());
            EXPECT_FALSE(harness.coordinator.includeSummary());
        }

        TEST(CaptureCoordinator, ReopeningResetsTheChoices) {
            Harness harness(realRender());
            int sourceCalls = 0;
            const CaptureSource source = countedSource(sourceCalls, tableSnapshotWithSummary());
            harness.coordinator.openPreview(CaptureTarget::BenchmarkTable, source);
            ASSERT_TRUE(isReadySoon(harness));
            harness.coordinator.setPercent(300);
            harness.coordinator.setIncludeSummary(false);
            ASSERT_EQ(harness.coordinator.percent(), 300);
            ASSERT_FALSE(harness.coordinator.includeSummary());

            harness.coordinator.closePreview();
            harness.coordinator.openPreview(CaptureTarget::BenchmarkTable, source);

            EXPECT_EQ(harness.coordinator.percent(), 200);
            EXPECT_TRUE(harness.coordinator.includeSummary());
        }

        TEST(CaptureCoordinator, ACommittedSizeScalesWithResolution) {
            Harness harness(realRender());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            harness.coordinator.setKeepAspect(false);

            harness.coordinator.commitDimension(Qt::Horizontal, QStringLiteral("1800"));
            harness.coordinator.commitDimension(Qt::Vertical, QStringLiteral("1000"));
            ASSERT_TRUE(isReadySoon(harness));

            EXPECT_EQ(harness.coordinator.outputSize(), QSize(1800, 1000));
            EXPECT_EQ(harness.coordinator.widthText(), QStringLiteral("1800"));
            EXPECT_EQ(harness.coordinator.heightText(), QStringLiteral("1000"));

            harness.coordinator.setPercent(300);
            ASSERT_TRUE(isReadySoon(harness));

            EXPECT_EQ(harness.coordinator.outputSize(), QSize(2700, 1500));
            EXPECT_EQ(harness.coordinator.widthText(), QStringLiteral("2700"));
            EXPECT_EQ(harness.coordinator.heightText(), QStringLiteral("1500"));
        }

        // A size that fits at one resolution can pass what an int holds at a higher one; the fields
        // must still show the choice the user is asked to correct.
        TEST(CaptureCoordinator, AnUnrepresentableSizeStaysInItsFields) {
            Harness harness(realRender());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            harness.coordinator.setKeepAspect(false);
            harness.coordinator.commitDimension(Qt::Horizontal, QStringLiteral("2000000000"));
            harness.coordinator.commitDimension(Qt::Vertical, QStringLiteral("1000"));
            drainUntilSettled(harness);

            harness.coordinator.setPercent(500);
            drainUntilSettled(harness);

            EXPECT_EQ(harness.coordinator.widthText().toLongLong(), 5'000'000'000LL);
            EXPECT_EQ(harness.coordinator.heightText().toLongLong(), 2'500LL);
            EXPECT_FALSE(harness.coordinator.previewError().isEmpty());
        }

        TEST(CaptureCoordinator, KeepAspectUpdatesTheOtherDimension) {
            Harness harness(realRender());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            const QSize initial = harness.coordinator.outputSize();

            harness.coordinator.commitDimension(Qt::Horizontal, QString::number(initial.width() * 2));
            EXPECT_EQ(harness.coordinator.heightText(), QString::number(initial.height() * 2));

            ASSERT_TRUE(isReadySoon(harness));
            EXPECT_EQ(harness.coordinator.outputSize(), initial * 2);
        }

        TEST(CaptureCoordinator, MalformedInputClearsTheImageAndKeepsTheText) {
            Harness harness(realRender());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));

            for (const QString &text : {QStringLiteral("abc"), QStringLiteral("0"), QStringLiteral("-5")}) {
                harness.coordinator.commitDimension(Qt::Horizontal, text);

                EXPECT_FALSE(harness.coordinator.widthError().isEmpty()) << text.toStdString();
                EXPECT_TRUE(harness.coordinator.heightError().isEmpty()) << text.toStdString();
                EXPECT_EQ(harness.coordinator.previewState(), PreviewState::Invalid) << text.toStdString();
                EXPECT_TRUE(harness.coordinator.previewImage().isNull()) << text.toStdString();
                EXPECT_FALSE(harness.coordinator.canOutput()) << text.toStdString();
                EXPECT_EQ(harness.coordinator.widthText(), text);
            }
        }

        TEST(CaptureCoordinator, AValidCorrectionRestoresThePreview) {
            const GraphCaptureSnapshot snapshot = graphSnapshot();
            const QSizeF reference = graphReferenceSize(snapshot, standardCaptureComposition());
            SteppedRender render(realRender());
            render.release(8);
            Harness harness(render.function());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, snapshot));
            ASSERT_TRUE(isReadySoon(harness));

            harness.coordinator.commitDimension(Qt::Horizontal, QStringLiteral("abc"));
            EXPECT_EQ(harness.coordinator.previewState(), PreviewState::Invalid);
            harness.coordinator.setPercent(300);
            QTest::qWait(kQuietWindowMs);
            EXPECT_EQ(render.started(), 1);

            harness.coordinator.commitDimension(Qt::Horizontal, QString::number(scaled(reference, 3.0).width()));

            ASSERT_TRUE(isReadySoon(harness));
            EXPECT_TRUE(harness.coordinator.canOutput());
            EXPECT_TRUE(harness.coordinator.widthError().isEmpty());
            EXPECT_EQ(harness.coordinator.outputSize(), scaled(reference, 3.0));
        }

        TEST(CaptureCoordinator, AnInFlightRenderCannotOutliveInvalidInput) {
            SteppedRender render(realRender());
            render.release();
            Harness harness(render.function());
            const auto releaseAll = qScopeGuard([&] { render.release(8); });
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));

            harness.coordinator.setPercent(300);
            harness.coordinator.commitDimension(Qt::Horizontal, QStringLiteral("abc"));
            render.release();
            ASSERT_TRUE(drainUntil([&] { return render.finished() == 2; }));
            QTest::qWait(kQuietWindowMs);

            EXPECT_EQ(harness.coordinator.previewState(), PreviewState::Invalid);
            EXPECT_TRUE(harness.coordinator.previewImage().isNull());
            EXPECT_FALSE(harness.coordinator.canOutput());
        }

        TEST(CaptureCoordinator, ARejectionClearsTheImageAndExplains) {
            const CaptureRejection rejection{CaptureRejectionReason::EdgeLimit,
                                             CaptureCorrection::ReduceDimensionsOrResolution, std::nullopt};
            ScriptedRender render(realRender());
            Harness harness(render.function());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));

            render.next(CaptureRejected{rejection, QSizeF()});
            harness.coordinator.setPercent(500);
            drainUntilSettled(harness);

            EXPECT_TRUE(harness.coordinator.previewImage().isNull());
            EXPECT_EQ(harness.coordinator.previewState(), PreviewState::Invalid);
            EXPECT_EQ(harness.coordinator.previewError(), captureRejectionText(rejection));
            EXPECT_FALSE(harness.coordinator.canOutput());
            EXPECT_EQ(harness.coordinator.percent(), 500);
        }

        TEST(CaptureCoordinator, ARoomRejectionIsHeldAgainstTheDimensions) {
            const CaptureRejection room{CaptureRejectionReason::TooLittleRoom, CaptureCorrection::LargerDimensions,
                                        std::nullopt};
            const CaptureRejection edge{CaptureRejectionReason::EdgeLimit,
                                        CaptureCorrection::ReduceDimensionsOrResolution, std::nullopt};
            ScriptedRender render(realRender());
            Harness harness(render.function());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));

            render.next(CaptureRejected{room, QSizeF()});
            harness.coordinator.commitDimension(Qt::Horizontal, QStringLiteral("400"));
            drainUntilSettled(harness);

            EXPECT_EQ(harness.coordinator.widthText(), QStringLiteral("400"));
            EXPECT_EQ(harness.coordinator.previewError(), captureRejectionText(room));
            EXPECT_FALSE(harness.coordinator.widthError().isEmpty());
            EXPECT_TRUE(harness.coordinator.heightError().isEmpty());

            render.next(CaptureRejected{edge, QSizeF()});
            harness.coordinator.commitDimension(Qt::Vertical, QStringLiteral("9000"));
            drainUntilSettled(harness);

            EXPECT_FALSE(harness.coordinator.heightError().isEmpty());
            EXPECT_TRUE(harness.coordinator.widthError().isEmpty());
        }

        TEST(CaptureCoordinator, RetryRendersTheSameSnapshotAndChoices) {
            ScriptedRender render(realRender());
            Harness harness(render.function());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));

            render.next(CaptureRenderFailed{});
            harness.coordinator.setPercent(300);
            drainUntilSettled(harness);
            EXPECT_EQ(harness.coordinator.previewState(), PreviewState::Failed);

            harness.coordinator.retryRender();
            ASSERT_TRUE(isReadySoon(harness));

            EXPECT_EQ(sourceCalls, 1);
            const std::vector<CaptureRenderRequest> requests = render.requests();
            ASSERT_EQ(requests.size(), 3u);
            EXPECT_EQ(requests[2].content, requests[1].content);
            EXPECT_EQ(requests[2].options.percent, requests[1].options.percent);
        }

        TEST(CaptureCoordinator, PreviewCopyUsesTheReviewedPixels) {
            TrackedRender render(realRender());
            Harness harness(render.function());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            harness.coordinator.setPercent(300);
            ASSERT_TRUE(isReadySoon(harness));
            const int rendersBeforeCopy = render.renders();

            harness.coordinator.copyPreview();
            drainUntil([&] { return harness.feedback.kind() != CaptureFeedback::Kind::None; });

            ASSERT_EQ(harness.clipboard->publishCount(), 1u);
            EXPECT_EQ(harness.clipboard->seen().front().pixels, render.latestPixels());
            EXPECT_EQ(sourceCalls, 1);
            EXPECT_EQ(render.renders(), rendersBeforeCopy);
            EXPECT_TRUE(harness.coordinator.previewOpen());
            EXPECT_EQ(harness.coordinator.percent(), 300);
            EXPECT_TRUE(harness.coordinator.canOutput());
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Success);
        }

        TEST(CaptureCoordinator, AFailedPreviewCopyKeepsTheImageForRetry) {
            Harness harness(realRender());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            const qint64 image = harness.coordinator.previewImage().cacheKey();
            harness.clipboard->setStatus(application::ImageOutputStatus::ClipboardNotConfirmed);

            harness.coordinator.copyPreview();
            drainUntil([&] { return harness.feedback.kind() != CaptureFeedback::Kind::None; });

            EXPECT_EQ(harness.clipboard->publishCount(), 1u);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Failure);
            EXPECT_EQ(harness.coordinator.previewImage().cacheKey(), image);
            EXPECT_TRUE(harness.coordinator.canOutput());

            harness.clipboard->setStatus(application::ImageOutputStatus::Succeeded);
            harness.coordinator.copyPreview();
            drainUntil([&] { return harness.feedback.kind() != CaptureFeedback::Kind::Failure; });

            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Success);
        }

        TEST(CaptureCoordinator, CopyDuringASaveChangesNothing) {
            Harness harness(realRender());
            const auto openGate = qScopeGuard([&] { harness.png->openGate(); });
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            harness.png->closeGate();
            ASSERT_TRUE(harness.output->savePng(smallRaster(), "capture.png", "someone else").accepted);

            harness.coordinator.copyPreview();

            EXPECT_EQ(harness.clipboard->publishCount(), 0u);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);
            EXPECT_TRUE(saysSaving(harness.feedback));
            EXPECT_EQ(harness.coordinator.previewState(), PreviewState::Ready);
            EXPECT_TRUE(harness.coordinator.canOutput());
        }

        TEST(CaptureCoordinator, CopyBeforeAnEarlierCopyIsDeliveredDoesNotSaySaving) {
            Harness harness(realRender());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            ASSERT_TRUE(harness.output->copyImage(smallRaster(), "someone else").accepted);

            harness.coordinator.copyPreview();

            EXPECT_EQ(harness.clipboard->publishCount(), 1u);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);
            EXPECT_FALSE(saysSaving(harness.feedback));
        }

        TEST(CaptureCoordinator, BeginExportIsRefusedDuringASave) {
            Harness harness(realRender());
            const auto openGate = qScopeGuard([&] { harness.png->openGate(); });
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            harness.png->closeGate();
            ASSERT_TRUE(harness.output->savePng(smallRaster(), "capture.png", "someone else").accepted);

            EXPECT_FALSE(harness.coordinator.beginExport());

            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);
            EXPECT_TRUE(saysSaving(harness.feedback));
            EXPECT_FALSE(harness.coordinator.optionsLocked());
        }

        TEST(CaptureCoordinator, BeginExportBeforeACopyIsDeliveredDoesNotSaySaving) {
            Harness harness(realRender());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            ASSERT_TRUE(harness.output->copyImage(smallRaster(), "someone else").accepted);

            EXPECT_FALSE(harness.coordinator.beginExport());

            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);
            EXPECT_FALSE(saysSaving(harness.feedback));
        }

        TEST(CaptureCoordinator, ThePickerLocksChoicesAndCancelReleasesThem) {
            TrackedRender render(realRender());
            Harness harness(render.function());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            const qint64 image = harness.coordinator.previewImage().cacheKey();
            const int rendersBeforeExport = render.renders();

            ASSERT_TRUE(harness.coordinator.beginExport());
            EXPECT_TRUE(harness.coordinator.optionsLocked());

            harness.coordinator.setPercent(400);
            QTest::qWait(kQuietWindowMs);
            EXPECT_EQ(harness.coordinator.percent(), 200);
            EXPECT_EQ(render.renders(), rendersBeforeExport);

            harness.coordinator.cancelExport();
            EXPECT_FALSE(harness.coordinator.optionsLocked());
            EXPECT_EQ(harness.coordinator.previewImage().cacheKey(), image);
            EXPECT_TRUE(harness.coordinator.canOutput());
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::None);
        }

        TEST(CaptureCoordinator, ConfirmingClosesBeforeTheSaveFinishes) {
            const std::filesystem::path destination = "capture.png";
            TrackedRender render(realRender());
            Harness harness(render.function());
            const auto openGate = qScopeGuard([&] { harness.png->openGate(); });
            QSignalSpy closed(&harness.coordinator, &CaptureCoordinator::previewClosed);
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            harness.png->closeGate();

            EXPECT_TRUE(harness.coordinator.beginExport());
            harness.coordinator.confirmExport(destination);

            EXPECT_FALSE(harness.coordinator.previewOpen());
            EXPECT_EQ(closed.count(), 1);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::None);

            ASSERT_TRUE(drainUntil([&] { return harness.png->writeCount() == 1u; }));
            EXPECT_EQ(harness.png->seen().front().image.pixels, render.latestPixels());
            EXPECT_EQ(harness.png->seen().front().destination, destination);

            harness.png->openGate();
            drainUntil([&] { return harness.feedback.kind() != CaptureFeedback::Kind::None; });

            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Success);
            EXPECT_TRUE(harness.feedback.text().contains(captureTargetName(CaptureTarget::Run)));
            EXPECT_TRUE(harness.feedback.text().contains(QStringLiteral("export"), Qt::CaseInsensitive));
        }

        TEST(CaptureCoordinator, ASaveFailureIsReportedWithoutAPreview) {
            Harness harness(realRender());
            harness.png->setStatus(application::ImageOutputStatus::CommitFailed);
            int sourceCalls = 0;
            const CaptureSource source = countedSource(sourceCalls, graphSnapshot());
            harness.coordinator.openPreview(CaptureTarget::Run, source);
            ASSERT_TRUE(isReadySoon(harness));

            EXPECT_TRUE(harness.coordinator.beginExport());
            harness.coordinator.confirmExport("some-folder/capture.png");

            EXPECT_FALSE(harness.coordinator.previewOpen());
            drainUntil([&] { return harness.feedback.kind() != CaptureFeedback::Kind::None; });

            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Failure);
            EXPECT_TRUE(harness.feedback.text().contains(captureTargetName(CaptureTarget::Run)));
            EXPECT_FALSE(harness.feedback.text().contains(QStringLiteral("some-folder")));
            EXPECT_FALSE(harness.feedback.text().contains(QStringLiteral("capture.png")));
            EXPECT_FALSE(harness.coordinator.previewOpen());

            harness.coordinator.openPreview(CaptureTarget::Run, source);

            EXPECT_EQ(sourceCalls, 2);
            EXPECT_EQ(harness.coordinator.percent(), 200);
        }

        // The picker can hand back a location that is not a local file; the reviewed image must survive it.
        TEST(CaptureCoordinator, AnUnusableDestinationKeepsThePreview) {
            Harness harness(realRender());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            const qint64 image = harness.coordinator.previewImage().cacheKey();

            ASSERT_TRUE(harness.coordinator.beginExport());
            harness.coordinator.confirmExport({});

            EXPECT_TRUE(harness.coordinator.previewOpen());
            EXPECT_EQ(harness.coordinator.previewImage().cacheKey(), image);
            EXPECT_FALSE(harness.coordinator.optionsLocked());
            EXPECT_TRUE(harness.coordinator.canOutput());
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Failure);
            EXPECT_FALSE(harness.output->activeOutput().has_value());
            EXPECT_EQ(harness.png->writeCount(), 0u);
        }

        TEST(CaptureCoordinator, ARefusedSaveKeepsThePreview) {
            Harness harness(realRender());
            int sourceCalls = 0;
            harness.coordinator.openPreview(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            ASSERT_TRUE(isReadySoon(harness));
            const qint64 image = harness.coordinator.previewImage().cacheKey();
            ASSERT_TRUE(harness.coordinator.beginExport());
            ASSERT_TRUE(harness.output->copyImage(smallRaster(), "someone else").accepted);

            harness.coordinator.confirmExport("capture.png");

            EXPECT_TRUE(harness.coordinator.previewOpen());
            EXPECT_EQ(harness.coordinator.previewImage().cacheKey(), image);
            EXPECT_FALSE(harness.coordinator.optionsLocked());
            EXPECT_NE(harness.feedback.kind(), CaptureFeedback::Kind::None);
            ASSERT_TRUE(drainUntil([&] { return !harness.output->activeOutput().has_value(); }));
            EXPECT_TRUE(harness.coordinator.canOutput());
            EXPECT_EQ(harness.png->writeCount(), 0u);
        }

        TEST(CaptureCoordinator, ClosingStartsNoOutput) {
            GatedRender render(realRender());
            Harness harness(render.function());
            const auto openGate = qScopeGuard([&] { render.open(); });
            int sourceCalls = 0;
            const CaptureSource source = countedSource(sourceCalls, graphSnapshot());

            harness.coordinator.openPreview(CaptureTarget::Run, source);
            ASSERT_TRUE(isReadySoon(harness));
            harness.coordinator.closePreview();

            render.close();
            harness.coordinator.openPreview(CaptureTarget::Run, source);
            harness.coordinator.closePreview();
            render.open();
            QTest::qWait(kQuietWindowMs);

            EXPECT_EQ(harness.clipboard->publishCount(), 0u);
            EXPECT_EQ(harness.png->writeCount(), 0u);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::None);
        }

        // ── Quick save ──────────────────────────────────────────────────────

        TEST(CaptureCoordinator, QuickSaveAsksForADestinationBeforeRendering) {
            RecordingRender render;
            Harness harness(render.function());
            QSignalSpy requested(&harness.coordinator, &CaptureCoordinator::saveDestinationRequested);
            int sourceCalls = 0;

            harness.coordinator.quickSave(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            QTest::qWait(kQuietWindowMs);

            ASSERT_EQ(requested.count(), 1);
            EXPECT_EQ(requested.front().front().toString(), QStringLiteral("Pasu Track Invincible.png"));
            EXPECT_EQ(sourceCalls, 1);
            EXPECT_TRUE(render.requests().empty());
            EXPECT_TRUE(harness.coordinator.quickSavePending());
            EXPECT_FALSE(harness.coordinator.previewOpen());
        }

        TEST(CaptureCoordinator, ConfirmingAQuickSaveWritesTheDefaultImage) {
            const GraphCaptureSnapshot snapshot = graphSnapshot();
            const QSizeF reference = graphReferenceSize(snapshot, standardCaptureComposition());
            const std::filesystem::path destination = "capture.png";
            Harness harness(realRender());
            int sourceCalls = 0;

            harness.coordinator.quickSave(CaptureTarget::Run, countedSource(sourceCalls, snapshot));
            harness.coordinator.confirmQuickSave(destination);
            drainUntil([&] { return harness.feedback.kind() != CaptureFeedback::Kind::None; });

            ASSERT_EQ(harness.png->writeCount(), 1u);
            EXPECT_EQ(harness.png->seen().front().destination, destination);
            EXPECT_EQ(harness.png->seen().front().image.width, qRound(reference.width() * 2.0));
            EXPECT_EQ(harness.png->seen().front().image.height, qRound(reference.height() * 2.0));
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Success);
            EXPECT_TRUE(harness.feedback.text().contains(QStringLiteral("export"), Qt::CaseInsensitive));
            EXPECT_FALSE(harness.coordinator.quickSavePending());
            EXPECT_EQ(harness.clipboard->publishCount(), 0u);
        }

        TEST(CaptureCoordinator, CancellingAQuickSaveRendersNothingAndReopensAdmission) {
            RecordingRender render;
            Harness harness(render.function());
            int sourceCalls = 0;
            const CaptureSource source = countedSource(sourceCalls, graphSnapshot());

            harness.coordinator.quickSave(CaptureTarget::Run, source);
            harness.coordinator.cancelQuickSave();
            QTest::qWait(kQuietWindowMs);

            EXPECT_TRUE(render.requests().empty());
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::None);
            EXPECT_FALSE(harness.coordinator.quickSavePending());

            harness.coordinator.openPreview(CaptureTarget::Run, source);
            EXPECT_TRUE(harness.coordinator.previewOpen());
        }

        TEST(CaptureCoordinator, AnUnusableQuickSaveDestinationIsReported) {
            RecordingRender render;
            Harness harness(render.function());
            int sourceCalls = 0;

            harness.coordinator.quickSave(CaptureTarget::Run, countedSource(sourceCalls, graphSnapshot()));
            harness.coordinator.confirmQuickSave({});
            QTest::qWait(kQuietWindowMs);

            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Failure);
            EXPECT_TRUE(harness.feedback.text().contains(captureTargetName(CaptureTarget::Run)));
            EXPECT_TRUE(render.requests().empty());
            EXPECT_FALSE(harness.coordinator.quickSavePending());
        }

        TEST(CaptureCoordinator, OtherCapturesWaitForTheChosenDestination) {
            Harness harness(failRender);
            QSignalSpy requested(&harness.coordinator, &CaptureCoordinator::saveDestinationRequested);
            int sourceCalls = 0;
            const CaptureSource source = countedSource(sourceCalls, graphSnapshot());
            harness.coordinator.quickSave(CaptureTarget::Run, source);

            harness.coordinator.quickCopy(CaptureTarget::DailyPlaytime, source);
            harness.coordinator.openPreview(CaptureTarget::Run, source);
            harness.coordinator.quickSave(CaptureTarget::Run, source);

            EXPECT_EQ(sourceCalls, 1);
            EXPECT_EQ(requested.count(), 1);
            EXPECT_FALSE(harness.coordinator.previewOpen());
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);
        }

        TEST(CaptureCoordinator, QuickSaveIsRejectedWhileAPreviewIsOpen) {
            RecordingRender render;
            Harness harness(render.function());
            QSignalSpy requested(&harness.coordinator, &CaptureCoordinator::saveDestinationRequested);
            int sourceCalls = 0;
            const CaptureSource source = countedSource(sourceCalls, graphSnapshot());
            harness.coordinator.openPreview(CaptureTarget::Run, source);

            harness.coordinator.quickSave(CaptureTarget::Run, source);

            EXPECT_EQ(requested.count(), 0);
            EXPECT_EQ(sourceCalls, 1);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);
        }

        TEST(CaptureCoordinator, QuickSaveIsRejectedWhileACopyPrepares) {
            GatedRender render(realRender());
            render.close();
            Harness harness(render.function());
            const auto openGate = qScopeGuard([&] { render.open(); });
            QSignalSpy requested(&harness.coordinator, &CaptureCoordinator::saveDestinationRequested);
            int sourceCalls = 0;
            const CaptureSource source = countedSource(sourceCalls, graphSnapshot());
            harness.coordinator.quickCopy(CaptureTarget::Run, source);

            harness.coordinator.quickSave(CaptureTarget::Run, source);

            EXPECT_EQ(requested.count(), 0);
            EXPECT_EQ(sourceCalls, 1);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Notice);
        }

        TEST(CaptureCoordinator, QuickSaveWithNothingToCaptureAsksForNoDestination) {
            Harness harness(failRender);
            QSignalSpy requested(&harness.coordinator, &CaptureCoordinator::saveDestinationRequested);
            int sourceCalls = 0;

            harness.coordinator.quickSave(CaptureTarget::Run, countedSource(sourceCalls, std::nullopt));

            EXPECT_EQ(requested.count(), 0);
            EXPECT_EQ(harness.feedback.kind(), CaptureFeedback::Kind::Failure);
            EXPECT_FALSE(harness.coordinator.quickSavePending());
        }
    }
}
