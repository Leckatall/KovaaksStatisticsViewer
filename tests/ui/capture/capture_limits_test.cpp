#include <gtest/gtest.h>

#include <QSize>
#include <QSizeF>
#include <QString>

#include "capture/capture_limits.h"
#include "capture/capture_sizing.h"

namespace ksv::ui {
    namespace {
        constexpr qint64 kBytesPerPixel = 4;

        CaptureLimits smallLimits() {
            CaptureLimits limits;
            limits.maxEdgePixels = 1000;
            limits.maxPixelArea = 400'000;
            limits.maxEstimatedBytes = 1'000'000'000;
            limits.maxSourcePoints = 500;
            limits.thumbnailBytes = 1000;
            return limits;
        }

        CaptureLimits wideEdgeLimits() {
            CaptureLimits limits = smallLimits();
            limits.maxEdgePixels = 16'384;
            return limits;
        }

        bool mentions(const QString &text, const QString &needle) {
            return text.contains(needle, Qt::CaseInsensitive);
        }
    }

    TEST(CaptureLimits, RejectsEachConstraintByName) {
        const CaptureLimits limits = smallLimits();
        CaptureLimits memoryLimits = limits;
        const QSize baseline(400, 300);
        const auto baselineBytes = estimateCaptureBytes(baseline, 0, memoryLimits);
        ASSERT_TRUE(baselineBytes.has_value());
        memoryLimits.maxEstimatedBytes = *baselineBytes;

        EXPECT_FALSE(validateCaptureOutput(CaptureSubject::Graph, QSizeF(baseline), 100, 0, memoryLimits).has_value());

        const auto edge = validateCaptureOutput(CaptureSubject::Graph, QSizeF(1001, 100), 100, 0, limits);
        ASSERT_TRUE(edge.has_value());
        EXPECT_EQ(edge->reason, CaptureRejectionReason::EdgeLimit);
        EXPECT_EQ(edge->correction, CaptureCorrection::ReduceDimensionsOrResolution);
        EXPECT_FALSE(edge->suggestedPercent.has_value());

        const auto area = validateCaptureOutput(CaptureSubject::Graph, QSizeF(800, 600), 100, 0, limits);
        ASSERT_TRUE(area.has_value());
        EXPECT_EQ(area->reason, CaptureRejectionReason::AreaLimit);
        EXPECT_EQ(area->correction, CaptureCorrection::ReduceDimensionsOrResolution);
        EXPECT_FALSE(area->suggestedPercent.has_value());

        const auto memory = validateCaptureOutput(CaptureSubject::Graph, QSizeF(baseline), 100, 100, memoryLimits);
        ASSERT_TRUE(memory.has_value());
        EXPECT_EQ(memory->reason, CaptureRejectionReason::MemoryLimit);
        EXPECT_EQ(memory->correction, CaptureCorrection::ReduceDimensionsOrResolution);
        EXPECT_FALSE(memory->suggestedPercent.has_value());

        const auto source = validateCaptureOutput(CaptureSubject::Graph, QSizeF(baseline), 100, 501, limits);
        ASSERT_TRUE(source.has_value());
        EXPECT_EQ(source->reason, CaptureRejectionReason::SourceGeometry);
        EXPECT_EQ(source->correction, CaptureCorrection::TooMuchData);
        EXPECT_TRUE(mentions(captureRejectionText(*source), QStringLiteral("too much data")));
    }

    TEST(CaptureLimits, UnrepresentableOutputIsAnArithmeticRejection) {
        const auto rejection = validateCaptureOutput(CaptureSubject::Graph, QSizeF(1e10, 10), 100, 0, smallLimits());
        ASSERT_TRUE(rejection.has_value());
        EXPECT_EQ(rejection->reason, CaptureRejectionReason::ArithmeticOverflow);
        EXPECT_EQ(rejection->correction, CaptureCorrection::ReduceDimensionsOrResolution);
        EXPECT_FALSE(rejection->suggestedPercent.has_value());
    }

    TEST(CaptureLimits, TheEstimateCountsEveryRetainedBuffer) {
        const CaptureLimits limits = smallLimits();
        const auto small = estimateCaptureBytes(QSize(1000, 1000), 0, limits);
        const auto doubled = estimateCaptureBytes(QSize(2000, 1000), 0, limits);
        ASSERT_TRUE(small.has_value());
        ASSERT_TRUE(doubled.has_value());
        EXPECT_GE(*doubled - *small, 3 * kBytesPerPixel * 1000 * 1000);

        const auto withPoints = estimateCaptureBytes(QSize(1000, 1000), 1000, limits);
        ASSERT_TRUE(withPoints.has_value());
        EXPECT_GT(*withPoints, *small);

        CaptureLimits bigThumbnail = limits;
        bigThumbnail.thumbnailBytes += 5000;
        const auto withThumbnail = estimateCaptureBytes(QSize(1000, 1000), 0, bigThumbnail);
        ASSERT_TRUE(withThumbnail.has_value());
        EXPECT_GE(*withThumbnail - *small, 5000);
    }

    TEST(CaptureLimits, ATableUnsupportedAtTheMinimumSaysSo) {
        const CaptureLimits limits = wideEdgeLimits();
        const QSizeF logical(2000, 1000);
        const auto atMinimum = captureOutputPixels(logical, kMinimumCapturePercent);
        ASSERT_TRUE(atMinimum.has_value());
        ASSERT_GT(qint64(atMinimum->width()) * atMinimum->height(), limits.maxPixelArea);

        const auto table = validateCaptureOutput(CaptureSubject::Table, logical, 200, 0, limits);
        ASSERT_TRUE(table.has_value());
        EXPECT_EQ(table->reason, CaptureRejectionReason::AreaLimit);
        EXPECT_EQ(table->correction, CaptureCorrection::UnsupportedAtMinimum);
        EXPECT_FALSE(table->suggestedPercent.has_value());
        EXPECT_TRUE(mentions(captureRejectionText(*table), QStringLiteral("minimum")));

        const auto graph = validateCaptureOutput(CaptureSubject::Graph, logical, 200, 0, limits);
        ASSERT_TRUE(graph.has_value());
        EXPECT_EQ(graph->correction, CaptureCorrection::ReduceDimensionsOrResolution);
    }

    TEST(CaptureLimits, ATableLowerResolutionIsNamedOnlyWhenItPasses) {
        const CaptureLimits limits = wideEdgeLimits();
        const QSizeF logical(400, 250);
        ASSERT_FALSE(validateCaptureOutput(CaptureSubject::Table, logical, 200, 0, limits).has_value());
        ASSERT_TRUE(validateCaptureOutput(CaptureSubject::Table, logical, 300, 0, limits).has_value());

        const auto rejection = validateCaptureOutput(CaptureSubject::Table, logical, 400, 0, limits);
        ASSERT_TRUE(rejection.has_value());
        EXPECT_EQ(rejection->reason, CaptureRejectionReason::AreaLimit);
        EXPECT_EQ(rejection->correction, CaptureCorrection::ReduceResolution);
        ASSERT_TRUE(rejection->suggestedPercent.has_value());
        EXPECT_EQ(*rejection->suggestedPercent, 200);
    }
}
