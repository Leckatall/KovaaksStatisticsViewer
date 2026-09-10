#include "benchmark_format.h"

#include <QLocale>
#include <cmath>

namespace ksv::presentation {
    QColor toQColor(const domain::BenchmarkColor &color) {
        return {color.red, color.green, color.blue, color.alpha};
    }

    QString formatBenchmarkNumber(const double value) {
        return QLocale().toString(std::round(value * 100.0) / 100.0);
    }
}
