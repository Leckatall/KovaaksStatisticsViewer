#include "capture/capture_appearance.h"

#include <QFontInfo>

#include <cmath>

namespace ksv::ui {
    namespace {
        // Qt's logical DPI for points on Windows, fixed so capture metrics do not depend on the screen.
        constexpr qreal kLogicalDpi = 96.0;
        constexpr qreal kPointsPerInch = 72.0;

        QColor colorProperty(const QObject *object, const char *name) {
            return object ? object->property(name).value<QColor>() : QColor();
        }

        CaptureFontDescription describeFont(const QFont &font) {
            CaptureFontDescription description;
            description.families = font.families();
            if (description.families.isEmpty()) description.families = {QFontInfo(font).family()};
            description.pixelSize = font.pixelSize() > 0
                                        ? font.pixelSize()
                                        : static_cast<int>(std::lround(font.pointSizeF() * kLogicalDpi / kPointsPerInch));
            return description;
        }
    }

    CaptureAppearance captureAppearanceFromTheme(const QObject &theme, const QFont &font) {
        const auto *palette = theme.property("palette").value<QObject *>();
        CaptureAppearance appearance;
        appearance.window = colorProperty(palette, "window");
        appearance.base = colorProperty(palette, "base");
        appearance.text = colorProperty(palette, "text");
        appearance.button = colorProperty(palette, "button");
        appearance.buttonText = colorProperty(palette, "buttonText");
        appearance.mid = colorProperty(palette, "mid");
        appearance.placeholderText = colorProperty(palette, "placeholderText");
        appearance.highlight = colorProperty(palette, "highlight");
        appearance.attention = colorProperty(&theme, "attention");
        appearance.groupTintAlpha = theme.property("groupTintAlpha").toReal();
        appearance.font = describeFont(font);
        return appearance;
    }
}
