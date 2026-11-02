#pragma once

#include <QFont>

#include "capture/capture_font_description.h"

namespace ksv::ui {
    enum class CaptureTextRole { Title, Context, Body, BodyBold, Small, Tick };

    QFont captureFont(const CaptureFontDescription &description, CaptureTextRole role);
}
