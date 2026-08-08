//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_VALUE_TRANSFORM_H
#define KOVAAKSSTATSVIEWER_VALUE_TRANSFORM_H

#include <QString>
#include <QtGlobal>
#include <cmath>
#include <functional>

namespace ksv::presentation {
    // Affine map from a plotted (raw) value to the value shown in axes/labels/
    // tooltips, so a series can be plotted in one unit and presented in another.
    struct ValueTransform {
        qreal scale = 1.0;
        qreal offset = 0.0;
        std::function<QString(qreal)> formatter;

        [[nodiscard]] qreal display(const qreal plotValue) const { return plotValue * scale + offset; }

        [[nodiscard]] QString format(const qreal displayValue) const {
            if (formatter) return formatter(displayValue);
            return QString::number(displayValue, 'f', std::abs(displayValue - std::round(displayValue)) < 1e-6 ? 0 : 1);
        }

        [[nodiscard]] static ValueTransform identity() { return {}; }

        [[nodiscard]] static ValueTransform percentage() {
            ValueTransform t;
            t.scale = 100.0;
            t.formatter = [](const qreal v) { return QString::number(std::round(v)) + "%"; };
            return t;
        }

        [[nodiscard]] static ValueTransform secondsToMinutes() {
            ValueTransform t;
            t.scale = 1.0 / 60.0;
            t.formatter = [](const qreal v) { return QString::number(std::round(v)) + " min"; };
            return t;
        }
    };
}

#endif //KOVAAKSSTATSVIEWER_VALUE_TRANSFORM_H
