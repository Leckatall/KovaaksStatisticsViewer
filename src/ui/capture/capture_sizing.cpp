#include "capture/capture_sizing.h"

#include <cmath>
#include <limits>

namespace ksv::ui {
    namespace {
        std::optional<int> roundedEdge(double logical, int percent) {
            const double scaled = logical * (percent / 100.0);
            if (!std::isfinite(scaled) || scaled < 0.5 || scaled >= static_cast<double>(std::numeric_limits<int>::max())) {
                return std::nullopt;
            }
            const long long rounded = std::llround(scaled);
            if (rounded < 1 || rounded > std::numeric_limits<int>::max()) {
                return std::nullopt;
            }
            return static_cast<int>(rounded);
        }

        bool usable(QSizeF size) {
            return std::isfinite(size.width()) && std::isfinite(size.height()) && size.width() > 0 && size.height() > 0;
        }
    }

    std::optional<QSize> captureOutputPixels(QSizeF logical, int percent) {
        if (percent <= 0 || !usable(logical)) {
            return std::nullopt;
        }
        const auto width = roundedEdge(logical.width(), percent);
        const auto height = roundedEdge(logical.height(), percent);
        if (!width || !height) {
            return std::nullopt;
        }
        return QSize(*width, *height);
    }

    // Rounding each pixel edge independently breaks exact doubling for a fractional edge, so the reference is whole.
    QSizeF wholeReferenceSize(QSizeF logical) {
        return {std::ceil(logical.width()), std::ceil(logical.height())};
    }

    QSizeF applyCustomDimension(QSizeF logical, Qt::Orientation edited, int pixels, int percent, bool keepAspect) {
        if (percent <= 0 || pixels <= 0 || !usable(logical)) {
            return logical;
        }
        const double edge = pixels / (percent / 100.0);
        const bool horizontal = edited == Qt::Horizontal;
        const double current = horizontal ? logical.width() : logical.height();
        const double ratio = edge / current;
        if (horizontal) {
            return {edge, keepAspect ? logical.height() * ratio : logical.height()};
        }
        return {keepAspect ? logical.width() * ratio : logical.width(), edge};
    }
}
