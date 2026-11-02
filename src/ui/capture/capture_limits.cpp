#include "capture/capture_limits.h"

#include <algorithm>
#include <limits>

#include "app/contracts/raster_image.h"
#include "capture/capture_sizing.h"

namespace ksv::ui {
    namespace {
        // Provisional: scene copy plus geometry built for painting, per observation or carry.
        constexpr qint64 kBytesPerSourcePoint = 48;
        // The current raster, an in-flight replacement, and the clipboard pixels retained after output.
        constexpr qint64 kRetainedRasters = 3;

        std::optional<qint64> checkedMul(const qint64 a, const qint64 b) {
            if (a < 0 || b < 0) return std::nullopt;
            if (a != 0 && b > std::numeric_limits<qint64>::max() / a) return std::nullopt;
            return a * b;
        }

        std::optional<qint64> checkedAdd(const qint64 a, const qint64 b) {
            if (a < 0 || b < 0 || a > std::numeric_limits<qint64>::max() - b) return std::nullopt;
            return a + b;
        }

        std::optional<CaptureRejectionReason> violatedConstraint(const QSizeF logical, const int percent,
                                                                  const qint64 sourcePoints,
                                                                  const CaptureLimits &limits) {
            const auto pixels = captureOutputPixels(logical, percent);
            if (!pixels) return CaptureRejectionReason::ArithmeticOverflow;
            if (std::max(pixels->width(), pixels->height()) > limits.maxEdgePixels) {
                return CaptureRejectionReason::EdgeLimit;
            }
            if (static_cast<qint64>(pixels->width()) * pixels->height() > limits.maxPixelArea) {
                return CaptureRejectionReason::AreaLimit;
            }
            if (sourcePoints > limits.maxSourcePoints) return CaptureRejectionReason::SourceGeometry;
            const auto bytes = estimateCaptureBytes(*pixels, sourcePoints, limits);
            if (!bytes || *bytes > limits.maxEstimatedBytes) return CaptureRejectionReason::MemoryLimit;
            return std::nullopt;
        }
    }

    CaptureLimits provisionalCaptureLimits() {
        CaptureLimits limits;
        limits.maxEdgePixels = 16'384;
        limits.maxPixelArea = 100'000'000;
        limits.maxEstimatedBytes = qint64{3} << 29;
        limits.maxSourcePoints = 2'000'000;
        limits.thumbnailBytes = qint64{16} << 20;
        return limits;
    }

    // PNG streams to its temporary file and Format_RGB32 needs no conversion, so the encoder adds no allocation.
    std::optional<qint64> estimateCaptureBytes(const std::optional<QSize> pixels, const qint64 sourcePoints,
                                               const CaptureLimits &limits) {
        if (!pixels || sourcePoints < 0) return std::nullopt;
        const auto raster = application::rasterByteSize(pixels->width(), pixels->height());
        if (!raster || *raster > static_cast<std::size_t>(std::numeric_limits<qint64>::max())) return std::nullopt;

        const auto rasters = checkedMul(static_cast<qint64>(*raster), kRetainedRasters);
        const auto source = checkedMul(sourcePoints, kBytesPerSourcePoint);
        if (!rasters || !source) return std::nullopt;
        const auto withSource = checkedAdd(*rasters, *source);
        if (!withSource) return std::nullopt;
        return checkedAdd(*withSource, limits.thumbnailBytes);
    }

    std::optional<CaptureRejection> validateCaptureOutput(const CaptureSubject subject, const QSizeF logical,
                                                          const int percent, const qint64 sourcePoints,
                                                          const CaptureLimits &limits) {
        const auto reason = violatedConstraint(logical, percent, sourcePoints, limits);
        if (!reason) return std::nullopt;

        CaptureRejection rejection;
        rejection.reason = *reason;
        if (*reason == CaptureRejectionReason::SourceGeometry) {
            rejection.correction = CaptureCorrection::TooMuchData;
        } else if (subject == CaptureSubject::Graph) {
            rejection.correction = CaptureCorrection::ReduceDimensionsOrResolution;
        } else {
            rejection.correction = CaptureCorrection::UnsupportedAtMinimum;
            for (auto mark = kCaptureResolutionMarks.rbegin(); mark != kCaptureResolutionMarks.rend(); ++mark) {
                if (*mark < percent && !violatedConstraint(logical, *mark, sourcePoints, limits)) {
                    rejection.correction = CaptureCorrection::ReduceResolution;
                    rejection.suggestedPercent = *mark;
                    break;
                }
            }
        }
        return rejection;
    }

    QString captureRejectionText(const CaptureRejection &rejection) {
        switch (rejection.correction) {
            case CaptureCorrection::LargerDimensions:
                return QStringLiteral("There is not enough room for the required content. Use larger dimensions.");
            case CaptureCorrection::LowerResolution:
                return QStringLiteral("There is not enough room for the required content at this resolution. "
                                      "Choose a lower resolution.");
            case CaptureCorrection::ReduceDimensionsOrResolution:
                return QStringLiteral("The image is too large to create. Reduce the dimensions or the resolution.");
            case CaptureCorrection::ReduceResolution:
                if (rejection.suggestedPercent) {
                    return QStringLiteral("The image is too large at this resolution. Choose %1% or lower.")
                            .arg(*rejection.suggestedPercent);
                }
                return QStringLiteral("The image is too large at this resolution. Choose a lower resolution.");
            case CaptureCorrection::UnsupportedAtMinimum:
                return QStringLiteral("This capture cannot be produced even at the minimum resolution of %1%.")
                        .arg(kMinimumCapturePercent);
            case CaptureCorrection::TooMuchData:
                return QStringLiteral("There is too much data for the graph to capture. "
                                      "Narrow the displayed range or show fewer series.");
        }
        return {};
    }
}
