#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QTest>

#include <gtest/gtest.h>

#include "app/contracts/image_output.h"
#include "app/contracts/raster_image.h"
#include "app/usecases/image_output_use_case.h"
#include "fake_image_output.h"

namespace ksv::application {
    namespace {
        constexpr int kResultDeadlineMs = 5000;
        constexpr int kQuietWindowMs = 100;
        const std::filesystem::path kDestination = "capture.png";

        RasterImage makeImage() {
            auto buffer = RasterBuffer::allocate(4, 3);
            EXPECT_TRUE(buffer.has_value());
            return std::move(*buffer).freeze();
        }

        struct Harness {
            std::shared_ptr<tests_support::FakeClipboardImageWriter> clipboard =
                    std::make_shared<tests_support::FakeClipboardImageWriter>();
            std::shared_ptr<tests_support::FakePngImageWriter> png =
                    std::make_shared<tests_support::FakePngImageWriter>();
            ImageOutputUseCase useCase{clipboard, png};
            std::vector<ImageOutputResult> results;

            Harness() {
                useCase.onCompleted([this](const ImageOutputResult &result) { results.push_back(result); });
            }

            // The use case joins its save worker on destruction, so a failed assertion that returns
            // with the gate closed would hang the suite instead of reporting.
            ~Harness() { png->openGate(); }

            [[nodiscard]] bool waitForResults(std::size_t count) {
                return QTest::qWaitFor([&] { return results.size() >= count; }, kResultDeadlineMs);
            }

            [[nodiscard]] bool waitForWriteToStart() const {
                return QTest::qWaitFor([&] { return png->writeCount() >= 1; }, kResultDeadlineMs);
            }

            // A save's result comes from another thread, so proving none follows takes a window of
            // processing rather than one synchronous flush.
            [[nodiscard]] bool noResultsBeyond(std::size_t count) {
                return !QTest::qWaitFor([&] { return results.size() > count; }, kQuietWindowMs);
            }
        };

        // A copy completes on this thread, so anything still owed is already a posted event and one
        // synchronous pass flushes it without a timed wait.
        void flushPostedEvents() {
            QCoreApplication::sendPostedEvents();
        }
    }

    TEST(ImageOutputUseCase, CopyPublishesBeforeReturningAndReportsThroughTheEventLoop) {
        Harness harness;
        const RasterImage image = makeImage();
        const std::string label = "Run graph";

        const ImageOutputAdmission admission = harness.useCase.copyImage(image, label);

        ASSERT_TRUE(admission.accepted);
        EXPECT_NE(admission.operationId, 0u);
        const auto seen = harness.clipboard->seen();
        ASSERT_EQ(seen.size(), 1u);
        EXPECT_EQ(seen[0].thread, std::this_thread::get_id());
        EXPECT_EQ(seen[0].pixels, image.pixels());
        EXPECT_TRUE(harness.results.empty());

        ASSERT_TRUE(harness.waitForResults(1));
        flushPostedEvents();
        ASSERT_EQ(harness.results.size(), 1u);
        EXPECT_EQ(harness.results[0].operationId, admission.operationId);
        EXPECT_EQ(harness.results[0].kind, ImageOutputKind::Copy);
        EXPECT_EQ(harness.results[0].targetLabel, label);
        EXPECT_EQ(harness.results[0].status, ImageOutputStatus::Succeeded);
    }

    TEST(ImageOutputUseCase, CopyFailureCarriesItsCategory) {
        Harness harness;
        harness.clipboard->setStatus(ImageOutputStatus::ClipboardNotConfirmed);

        const ImageOutputAdmission admission = harness.useCase.copyImage(makeImage(), "Run graph");

        ASSERT_TRUE(admission.accepted);
        ASSERT_TRUE(harness.waitForResults(1));
        EXPECT_EQ(harness.results[0].status, ImageOutputStatus::ClipboardNotConfirmed);

        flushPostedEvents();
        EXPECT_EQ(harness.results.size(), 1u);
    }

    TEST(ImageOutputUseCase, OperationIdsAreDistinct) {
        Harness harness;

        const ImageOutputAdmission first = harness.useCase.copyImage(makeImage(), "First");
        ASSERT_TRUE(first.accepted);
        ASSERT_TRUE(harness.waitForResults(1));

        const ImageOutputAdmission second = harness.useCase.copyImage(makeImage(), "Second");
        ASSERT_TRUE(second.accepted);
        ASSERT_TRUE(harness.waitForResults(2));

        EXPECT_NE(first.operationId, second.operationId);
        EXPECT_EQ(harness.results[0].operationId, first.operationId);
        EXPECT_EQ(harness.results[1].operationId, second.operationId);
    }

    // A handler is the first code able to react to a result, so admission has to be free by the time
    // handlers run, not only after they return.
    TEST(ImageOutputUseCase, ACopyAdmittedAfterCompletionIsAccepted) {
        Harness harness;
        ImageOutputAdmission chained;
        bool chainedOnce = false;
        harness.useCase.onCompleted([&](const ImageOutputResult &) {
            if (std::exchange(chainedOnce, true)) return;
            chained = harness.useCase.copyImage(makeImage(), "Chained");
        });

        const ImageOutputAdmission first = harness.useCase.copyImage(makeImage(), "First");

        ASSERT_TRUE(first.accepted);
        ASSERT_TRUE(harness.waitForResults(1));
        ASSERT_TRUE(chained.accepted);
        ASSERT_TRUE(harness.waitForResults(2));
        EXPECT_EQ(harness.results[1].operationId, chained.operationId);
    }

    TEST(ImageOutputUseCase, AThrowingClipboardReportsUnexpected) {
        Harness harness;
        harness.clipboard->setHook([] { throw std::runtime_error("clipboard gone"); });

        const ImageOutputAdmission admission = harness.useCase.copyImage(makeImage(), "Run graph");

        ASSERT_TRUE(admission.accepted);
        ASSERT_TRUE(harness.waitForResults(1));
        flushPostedEvents();
        ASSERT_EQ(harness.results.size(), 1u);
        EXPECT_EQ(harness.results[0].status, ImageOutputStatus::Unexpected);
        EXPECT_TRUE(harness.useCase.copyImage(makeImage(), "Following copy").accepted);
    }

    TEST(ImageOutputSave, WriteRunsOffTheCallingThreadAndReportsOnIt) {
        Harness harness;
        const std::string label = "Run graph";
        std::thread::id handlerThread;
        harness.useCase.onCompleted([&](const ImageOutputResult &) { handlerThread = std::this_thread::get_id(); });

        const ImageOutputAdmission admission = harness.useCase.savePng(makeImage(), kDestination, label);

        ASSERT_TRUE(admission.accepted);
        ASSERT_TRUE(harness.waitForResults(1));
        const auto seen = harness.png->seen();
        ASSERT_EQ(seen.size(), 1u);
        EXPECT_NE(seen[0].image.thread, std::this_thread::get_id());
        EXPECT_EQ(seen[0].destination, kDestination);
        EXPECT_EQ(handlerThread, std::this_thread::get_id());
        EXPECT_EQ(harness.results[0].operationId, admission.operationId);
        EXPECT_EQ(harness.results[0].kind, ImageOutputKind::SavePng);
        EXPECT_EQ(harness.results[0].targetLabel, label);
        EXPECT_EQ(harness.results[0].status, ImageOutputStatus::Succeeded);
    }

    TEST(ImageOutputSave, NoResultWhileTheWriteIsPending) {
        Harness harness;
        harness.png->closeGate();

        const ImageOutputAdmission admission = harness.useCase.savePng(makeImage(), kDestination, "Run graph");

        ASSERT_TRUE(admission.accepted);
        ASSERT_TRUE(harness.waitForWriteToStart());
        EXPECT_TRUE(harness.noResultsBeyond(0));

        harness.png->openGate();
        ASSERT_TRUE(harness.waitForResults(1));
        EXPECT_TRUE(harness.noResultsBeyond(1));
    }

    TEST(ImageOutputSave, AFailedWriteCarriesItsCategory) {
        Harness harness;
        harness.png->setStatus(ImageOutputStatus::CommitFailed);

        const ImageOutputAdmission admission = harness.useCase.savePng(makeImage(), kDestination, "Run graph");

        ASSERT_TRUE(admission.accepted);
        ASSERT_TRUE(harness.waitForResults(1));
        EXPECT_EQ(harness.results[0].status, ImageOutputStatus::CommitFailed);
        EXPECT_TRUE(harness.noResultsBeyond(1));
    }

    TEST(ImageOutputSave, AThrowingWriterStillCompletes) {
        Harness harness;
        harness.png->setHook([] { throw std::runtime_error("disk gone"); });

        const ImageOutputAdmission admission = harness.useCase.savePng(makeImage(), kDestination, "Run graph");

        ASSERT_TRUE(admission.accepted);
        ASSERT_TRUE(harness.waitForResults(1));
        EXPECT_EQ(harness.results[0].status, ImageOutputStatus::Unexpected);
        EXPECT_TRUE(harness.useCase.copyImage(makeImage(), "Following copy").accepted);
    }

    TEST(ImageOutputSave, OneOperationAtATimeAcrossTargets) {
        Harness harness;
        harness.png->closeGate();

        const ImageOutputAdmission save = harness.useCase.savePng(makeImage(), kDestination, "Admitted");

        ASSERT_TRUE(save.accepted);
        ASSERT_TRUE(harness.waitForWriteToStart());

        const ImageOutputAdmission copy = harness.useCase.copyImage(makeImage(), "Rejected copy");
        const ImageOutputAdmission secondSave = harness.useCase.savePng(makeImage(), kDestination, "Rejected save");

        EXPECT_FALSE(copy.accepted);
        EXPECT_EQ(copy.operationId, 0u);
        EXPECT_FALSE(secondSave.accepted);
        EXPECT_EQ(secondSave.operationId, 0u);
        EXPECT_EQ(harness.clipboard->publishCount(), 0u);

        harness.png->openGate();
        ASSERT_TRUE(harness.waitForResults(1));
        EXPECT_EQ(harness.results[0].operationId, save.operationId);
        EXPECT_TRUE(harness.noResultsBeyond(1));
        EXPECT_EQ(harness.png->writeCount(), 1u);

        EXPECT_TRUE(harness.useCase.copyImage(makeImage(), "After completion").accepted);
    }

    TEST(ImageOutputSave, ReleasesTheImageOnceDone) {
        Harness harness;
        RasterImage image = makeImage();
        const std::weak_ptr<const std::uint32_t[]> storage = image.storage();

        const ImageOutputAdmission admission = harness.useCase.savePng(std::move(image), kDestination, "Run graph");

        ASSERT_TRUE(admission.accepted);
        ASSERT_TRUE(harness.waitForResults(1));
        EXPECT_TRUE(storage.expired());
    }

    // Closing the app does not cancel a committed save, but nothing is left to receive its result.
    TEST(ImageOutputSave, DestructionWaitsForAnAcceptedSaveAndReportsNothing) {
        auto clipboard = std::make_shared<tests_support::FakeClipboardImageWriter>();
        auto png = std::make_shared<tests_support::FakePngImageWriter>();
        std::vector<ImageOutputResult> results;
        std::atomic<bool> writeFinished = false;
        // The delay keeps the write running past the destructor call if the destructor does not join.
        png->setHook([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            writeFinished = true;
        });
        auto useCase = std::make_unique<ImageOutputUseCase>(clipboard, png);
        useCase->onCompleted([&](const ImageOutputResult &result) { results.push_back(result); });

        const ImageOutputAdmission admission = useCase->savePng(makeImage(), kDestination, "Run graph");

        ASSERT_TRUE(admission.accepted);
        useCase.reset();
        EXPECT_TRUE(writeFinished);

        flushPostedEvents();
        EXPECT_TRUE(results.empty());
    }
}
