//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_VALUE_DELEGATE_H
#define KOVAAKSSTATSVIEWER_VALUE_DELEGATE_H

#include <QString>
#include <QtGlobal>
#include <cmath>
#include <functional>

namespace ksv::presentation {
    // How a raw value is shown: scale/offset converts it for display only (raw points stay raw, e.g.
    // seconds plotted, minutes shown); the formatter labels the converted value.
    struct ValueDelegate {
        qreal scale = 1.0;
        qreal offset = 0.0;
        std::function<QString(qreal)> axisFormatter;
        std::function<QString(qreal)> hoverFormatter;

        [[nodiscard]] qreal display(const qreal plotValue) const { return plotValue * scale + offset; }

        [[nodiscard]] QString axisLabel(const qreal displayValue) const {
            if (axisFormatter) return axisFormatter(displayValue);
            return QString::number(displayValue, 'f', std::abs(displayValue - std::round(displayValue)) < 1e-6 ? 0 : 1);
        }

        [[nodiscard]] QString hoverLabel(const qreal displayValue) const {
            return hoverFormatter ? hoverFormatter(displayValue) : axisLabel(displayValue);
        }

        [[nodiscard]] static ValueDelegate identity() { return {}; }

        [[nodiscard]] static ValueDelegate percentage() {
            ValueDelegate t;
            t.scale = 100.0;
            t.axisFormatter = [](const qreal v) { return QString::number(std::round(v)) + "%"; };
            return t;
        }

        [[nodiscard]] static ValueDelegate secondsToMinutes() {
            ValueDelegate t;
            t.scale = 1.0 / 60.0;
            t.axisFormatter = [](const qreal v) { return QString::number(std::round(v)) + " min"; };
            return t;
        }
    };
}

#endif //KOVAAKSSTATSVIEWER_VALUE_DELEGATE_H
