#pragma once

#include <QSize>
#include <QSizeF>
#include <Qt>

#include <optional>

namespace ksv::ui {
    std::optional<QSize> captureOutputPixels(QSizeF logical, int percent);

    QSizeF applyCustomDimension(QSizeF logical, Qt::Orientation edited, int pixels, int percent, bool keepAspect);
}
