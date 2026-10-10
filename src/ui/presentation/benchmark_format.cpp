#include "benchmark_format.h"

#include <QLocale>
#include <algorithm>
#include <cmath>

namespace ksv::presentation {
    namespace {
        double linearChannel(const double channel) {
            return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        }

        double relativeLuminance(const QColor &color) {
            return 0.2126 * linearChannel(color.redF()) + 0.7152 * linearChannel(color.greenF()) +
                   0.0722 * linearChannel(color.blueF());
        }

        constexpr double kMinimumTextContrast = 4.5;
    }

    QColor toQColor(const domain::BenchmarkColor &color) {
        return {color.red, color.green, color.blue, color.alpha};
    }

    QString formatBenchmarkNumber(const double value) {
        return QLocale().toString(std::round(value * 100.0) / 100.0);
    }

    QColor darkPanelColor() { return {0x34, 0x34, 0x34}; }

    double contrastRatio(const QColor &a, const QColor &b) {
        const double la = relativeLuminance(a);
        const double lb = relativeLuminance(b);
        return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
    }

    QColor readableTextColor(const domain::BenchmarkColor &color) {
        const QColor base(color.red, color.green, color.blue);
        const QColor panel = darkPanelColor();
        if (contrastRatio(base, panel) >= kMinimumTextContrast) return base;
        // Blend toward white in small steps; the first blend that reads is the closest to the
        // benchmark's own colour. White itself always passes against a dark panel.
        for (int step = 1; step <= 20; ++step) {
            const double t = step / 20.0;
            const QColor mixed(static_cast<int>(std::lround(base.red() + (255 - base.red()) * t)),
                               static_cast<int>(std::lround(base.green() + (255 - base.green()) * t)),
                               static_cast<int>(std::lround(base.blue() + (255 - base.blue()) * t)));
            if (contrastRatio(mixed, panel) >= kMinimumTextContrast) return mixed;
        }
        return Qt::white;
    }
}
