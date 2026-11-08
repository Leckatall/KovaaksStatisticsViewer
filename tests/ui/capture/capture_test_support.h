#pragma once

#include <QColor>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <variant>
#include <vector>

#include "capture/capture_fonts.h"
#include "capture/capture_layout.h"

namespace ksv::ui {
    inline std::vector<CaptureTextItem> captureTextItems(const std::vector<CaptureItem> &items) {
        std::vector<CaptureTextItem> texts;
        for (const CaptureItem &item : items) {
            if (const auto *text = std::get_if<CaptureTextItem>(&item)) texts.push_back(*text);
        }
        return texts;
    }

    inline std::vector<CaptureTextItem> captureTextItems(const std::vector<CaptureItem> &items,
                                                         const CaptureTextRole role) {
        std::vector<CaptureTextItem> texts;
        for (const CaptureTextItem &text : captureTextItems(items)) {
            if (text.role == role) texts.push_back(text);
        }
        return texts;
    }

    // Separated by newlines, so a contains-check cannot match across two measured lines.
    inline QString joinedCaptureText(const std::vector<CaptureItem> &items) {
        QStringList lines;
        for (const CaptureTextItem &text : captureTextItems(items)) lines.append(text.text);
        return lines.join(QLatin1Char('\n'));
    }

    inline QString joinedCaptureText(const std::vector<CaptureItem> &items, const CaptureTextRole role) {
        QStringList lines;
        for (const CaptureTextItem &text : captureTextItems(items, role)) lines.append(text.text);
        return lines.join(QLatin1Char('\n'));
    }

    inline qint64 countPixelsAtColor(const QImage &image, const QColor &color) {
        const QRgb wanted = color.rgb();
        qint64 count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (image.pixel(x, y) == wanted) ++count;
            }
        }
        return count;
    }
}
