#include "capture/capture_fonts.h"

#include <algorithm>
#include <cmath>

namespace ksv::ui {
    namespace {
        // Provisional ratios of the description's pixel size.
        double roleRatio(CaptureTextRole role) {
            switch (role) {
                case CaptureTextRole::Title: return 1.6;
                case CaptureTextRole::Context: return 1.15;
                case CaptureTextRole::Body:
                case CaptureTextRole::BodyBold: return 1.0;
                case CaptureTextRole::Small:
                case CaptureTextRole::SmallBold: return 0.85;
                case CaptureTextRole::Tick: return 0.8;
            }
            return 1.0;
        }
    }

    QFont captureFont(const CaptureFontDescription &description, CaptureTextRole role) {
        QFont font;
        font.setFamilies(description.families);
        font.setPixelSize(std::max(1, static_cast<int>(std::lround(description.pixelSize * roleRatio(role)))));
        font.setHintingPreference(QFont::PreferNoHinting);
        font.setStyleStrategy(QFont::NoSubpixelAntialias);
        if (role == CaptureTextRole::BodyBold || role == CaptureTextRole::SmallBold || role == CaptureTextRole::Title) {
            font.setWeight(QFont::Bold);
        }
        return font;
    }
}
