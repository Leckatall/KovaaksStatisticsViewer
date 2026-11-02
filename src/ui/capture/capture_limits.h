#pragma once

#include <QSize>
#include <QSizeF>
#include <QString>
#include <QtGlobal>

#include <array>
#include <optional>

namespace ksv::ui {
    constexpr int kMinimumCapturePercent = 50;
    constexpr std::array<int, 6> kCaptureResolutionMarks{50, 100, 200, 300, 400, 500};

    struct CaptureLimits {
        int maxEdgePixels = 0;
        qint64 maxPixelArea = 0;
        qint64 maxEstimatedBytes = 0;
        qint64 maxSourcePoints = 0;
        qint64 thumbnailBytes = 0;
    };

    CaptureLimits provisionalCaptureLimits();

    enum class CaptureRejectionReason { TooLittleRoom, EdgeLimit, AreaLimit, MemoryLimit, SourceGeometry, ArithmeticOverflow };

    enum class CaptureCorrection {
        LargerDimensions,
        LowerResolution,
        ReduceDimensionsOrResolution,
        ReduceResolution,
        UnsupportedAtMinimum,
        TooMuchData
    };

    struct CaptureRejection {
        CaptureRejectionReason reason = CaptureRejectionReason::TooLittleRoom;
        CaptureCorrection correction = CaptureCorrection::LargerDimensions;
        std::optional<int> suggestedPercent;
    };

    enum class CaptureSubject { Graph, Table };

    std::optional<qint64> estimateCaptureBytes(std::optional<QSize> pixels, qint64 sourcePoints,
                                               const CaptureLimits &limits);

    std::optional<CaptureRejection> validateCaptureOutput(CaptureSubject subject, QSizeF logical, int percent,
                                                          qint64 sourcePoints, const CaptureLimits &limits);

    QString captureRejectionText(const CaptureRejection &rejection);
}
