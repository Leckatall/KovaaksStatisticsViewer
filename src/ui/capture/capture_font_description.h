#pragma once

#include <QStringList>

namespace ksv::ui {
    // Apart from capture_snapshot.h so that layout and rendering stay clear of the view-model headers it needs.
    struct CaptureFontDescription {
        QStringList families;
        int pixelSize = 0;
    };
}
