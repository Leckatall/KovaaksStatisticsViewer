#ifndef KOVAAKSSTATSVIEWER_CAPTURE_APPEARANCE_H
#define KOVAAKSSTATSVIEWER_CAPTURE_APPEARANCE_H

#include <QFont>
#include <QObject>

#include "capture/capture_snapshot.h"

namespace ksv::ui {
    // `theme` is the QML `Theme` singleton, which exists only there; its tokens are read by property name.
    [[nodiscard]] CaptureAppearance captureAppearanceFromTheme(const QObject &theme, const QFont &font);
}

#endif // KOVAAKSSTATSVIEWER_CAPTURE_APPEARANCE_H
