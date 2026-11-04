#include "capture/capture_limits.h"

#include <QLocale>
#include <QObject>

#include <algorithm>
#include <limits>

#include "app/contracts/raster_image.h"
#include "capture/capture_sizing.h"

namespace ksv::ui {
    namespace {
        // Scene copy plus geometry built for painting, per observation or carry. Measured peak growth was 53 to 67.
        constexpr qint64 kBytesPerSourcePoint = 72;
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

        std::optional<qint64> violatedLimit(const CaptureRejectionReason reason, const CaptureLimits &limits) {
            switch (reason) {
                case CaptureRejectionReason::EdgeLimit:
                    return limits.maxEdgePixels;
                case CaptureRejectionReason::AreaLimit:
                    return limits.maxPixelArea;
                case CaptureRejectionReason::MemoryLimit:
                    return limits.maxEstimatedBytes;
                case CaptureRejectionReason::SourceGeometry:
                    return limits.maxSourcePoints;
                case CaptureRejectionReason::TooLittleRoom:
                case CaptureRejectionReason::ArithmeticOverflow:
                    return std::nullopt;
            }
            return std::nullopt;
        }
    }

    CaptureLimits standardCaptureLimits() {
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
        rejection.limit = violatedLimit(*reason, limits);
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

    namespace {
        QString limitText(const CaptureRejection &rejection) {
            const QLocale locale;
            switch (rejection.reason) {
                case CaptureRejectionReason::TooLittleRoom:
                    return QObject::tr("The required content does not fit the available space.");
                case CaptureRejectionReason::EdgeLimit:
                    if (rejection.limit) {
                        return QObject::tr("An edge of the image is longer than the maximum of %1 pixels.")
                                .arg(locale.toString(*rejection.limit));
                    }
                    return QObject::tr("An edge of the image is longer than the maximum allowed.");
                case CaptureRejectionReason::AreaLimit:
                    if (rejection.limit) {
                        return QObject::tr("The image has more than the maximum of %1 pixels.")
                                .arg(locale.toString(*rejection.limit));
                    }
                    return QObject::tr("The image has more pixels than the maximum allowed.");
                case CaptureRejectionReason::MemoryLimit:
                    if (rejection.limit) {
                        return QObject::tr("The image needs more than the %1 of memory allowed.")
                                .arg(locale.formattedDataSize(*rejection.limit));
                    }
                    return QObject::tr("The image needs more memory than is allowed.");
                case CaptureRejectionReason::SourceGeometry:
                    if (rejection.limit) {
                        return QObject::tr("The graph holds more than the %1 points that can be captured.")
                                .arg(locale.toString(*rejection.limit));
                    }
                    return QObject::tr("The graph holds more points than can be captured.");
                case CaptureRejectionReason::ArithmeticOverflow:
                    return QObject::tr("The image size is beyond what can be represented.");
            }
            return {};
        }

        QString correctionText(const CaptureRejection &rejection) {
            switch (rejection.correction) {
                case CaptureCorrection::LargerDimensions:
                    return QObject::tr("Use larger dimensions.");
                case CaptureCorrection::LowerResolution:
                    return QObject::tr("Choose a lower resolution.");
                case CaptureCorrection::ReduceDimensionsOrResolution:
                    return QObject::tr("Reduce the dimensions or the resolution.");
                case CaptureCorrection::ReduceResolution:
                    if (rejection.suggestedPercent) {
                        return QObject::tr("Choose %1% or lower.").arg(*rejection.suggestedPercent);
                    }
                    return QObject::tr("Choose a lower resolution.");
                case CaptureCorrection::UnsupportedAtMinimum:
                    return QObject::tr("This capture cannot be produced even at the minimum resolution of %1%.")
                            .arg(kMinimumCapturePercent);
                case CaptureCorrection::TooMuchData:
                    return QObject::tr("There is too much data for the graph to capture. "
                                       "Narrow the displayed range or show fewer series.");
            }
            return {};
        }
    }

    QString captureRejectionText(const CaptureRejection &rejection) {
        return QObject::tr("%1 %2").arg(limitText(rejection), correctionText(rejection));
    }
}
