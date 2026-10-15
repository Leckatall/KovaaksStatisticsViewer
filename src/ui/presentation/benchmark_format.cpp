#include "benchmark_format.h"

#include <QLocale>
#include <QObject>
#include <algorithm>
#include <cmath>
#include <cstddef>

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

    QString describeAverageRank(const double rank, const std::vector<QString> &tierNames) {
        if (tierNames.empty()) return formatBenchmarkNumber(rank);
        const double tierCount = static_cast<double>(tierNames.size());
        // A mean of whole ranks can land a rounding error short of a tier; snap it onto the tier.
        constexpr double kSnap = 1e-9;
        double clamped = std::clamp(rank, 0.0, tierCount);
        if (std::abs(clamped - std::round(clamped)) < kSnap) clamped = std::round(clamped);
        const auto attained = static_cast<std::size_t>(std::floor(clamped));
        const QString current = attained == 0 ? QObject::tr("Unranked") : tierNames[attained - 1];
        if (attained >= tierNames.size()) return QObject::tr("%1 · top tier reached").arg(current);
        const int percent = std::min(99, static_cast<int>(std::floor((clamped - std::floor(clamped)) * 100.0 + kSnap)));
        return QObject::tr("%1 · %2% to %3").arg(current).arg(percent).arg(tierNames[attained]);
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
