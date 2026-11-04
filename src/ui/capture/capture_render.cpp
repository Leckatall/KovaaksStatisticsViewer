#include "capture/capture_render.h"

#include <QtMath>

#include <type_traits>
#include <utility>

#include "capture/capture_renderer.h"
#include "capture/capture_sizing.h"
#include "capture/table_capture_layout.h"

namespace ksv::ui {
    namespace {
        struct Measured {
            CaptureLayout layout;
            qint64 sourcePoints = 0;
            CaptureSubject subject = CaptureSubject::Graph;
        };

        std::variant<Measured, CaptureRejected> measureGraph(const GraphCaptureSnapshot &snapshot,
                                                             const CaptureOptions &options,
                                                             const CaptureLimits &limits,
                                                             const CaptureComposition &composition) {
            const QSizeF logical = options.logical.value_or(graphReferenceSize(snapshot, composition));
            auto result = layoutGraphCapture(snapshot, logical, composition);
            if (auto *layout = std::get_if<CaptureLayout>(&result)) {
                return Measured{std::move(*layout), captureSourcePoints(snapshot), CaptureSubject::Graph};
            }
            CaptureRejection rejection = std::get<CaptureRejection>(result);
            if (options.logical && rejection.reason == CaptureRejectionReason::TooLittleRoom) {
                const std::optional<QSize> pixels = captureOutputPixels(logical, options.percent);
                if (pixels) {
                    rejection = suggestGraphResolution(snapshot, *pixels, options.percent, composition, limits)
                                        .value_or(rejection);
                }
            }
            return CaptureRejected{rejection, logical};
        }

        // A smooth downscale of a wrapper over the raster, so nothing is copied at full size.
        QImage makeThumbnail(const application::RasterImage &raster, const qint64 allowance) {
            const QImage source(reinterpret_cast<const uchar *>(raster.pixels()), raster.width(), raster.height(),
                                static_cast<qsizetype>(raster.stride()), QImage::Format_RGB32);
            const qreal scale = qMin(1.0, qSqrt(static_cast<qreal>(allowance) / static_cast<qreal>(raster.byteSize())));
            const QSize size(qMax(1, qFloor(raster.width() * scale)), qMax(1, qFloor(raster.height() * scale)));
            if (size == source.size()) return source.copy();
            return source.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        CaptureRenderOutcome renderRequest(const CaptureRenderRequest &request, const CaptureLimits &limits,
                                           const CaptureComposition &composition) {
            const CaptureOptions &options = request.options;
            std::variant<Measured, CaptureRejected> measured = std::visit(
                    [&](const auto &snapshot) -> std::variant<Measured, CaptureRejected> {
                        using Snapshot = std::decay_t<decltype(snapshot)>;
                        if constexpr (std::is_same_v<Snapshot, GraphCaptureSnapshot>) {
                            return measureGraph(snapshot, options, limits, composition);
                        } else {
                            return Measured{layoutTableCapture(snapshot, options.includeSummary), 0,
                                            CaptureSubject::Table};
                        }
                    },
                    *request.content);
            if (auto *rejected = std::get_if<CaptureRejected>(&measured)) return *rejected;

            Measured &ready = std::get<Measured>(measured);
            const QSizeF logical = ready.layout.size;
            if (const std::optional<CaptureRejection> rejection = validateCaptureOutput(
                        ready.subject, logical, options.percent, ready.sourcePoints, limits)) {
                return CaptureRejected{*rejection, logical};
            }
            const std::optional<QSize> pixels = captureOutputPixels(logical, options.percent);
            if (!pixels) return CaptureRenderFailed{};

            std::optional<application::RasterImage> raster = renderCapture(ready.layout, *pixels, request.cancelled.get());
            if (!raster) return CaptureRenderFailed{};
            QImage thumbnail = makeThumbnail(*raster, limits.thumbnailBytes);
            if (thumbnail.isNull()) return CaptureRenderFailed{};
            return CaptureRendered{*std::move(raster), std::move(thumbnail), *pixels, logical};
        }
    }

    CaptureRenderFunction makeCaptureRenderFunction(CaptureLimits limits, CaptureComposition composition) {
        return [limits, composition](const CaptureRenderRequest &request) -> CaptureRenderOutcome {
            try {
                return renderRequest(request, limits, composition);
            } catch (...) {
                return CaptureRenderFailed{};
            }
        };
    }
}
