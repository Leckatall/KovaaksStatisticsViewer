#ifndef KOVAAKSSTATSVIEWER_QML_REGISTRATION_H
#define KOVAAKSSTATSVIEWER_QML_REGISTRATION_H

#include <QtQml>

#include <ui/components/graph_canvas.h>
#include <presentation/graph_vm.h>
#include <presentation/playtime_graph_vm.h>

namespace ksv {
    // Referencing these types keeps the linker from dropping the static QML
    // module's auto-registration objects (ksv_ui is built NO_PLUGIN). Called
    // from every executable that loads the QML module.
    inline void declare_metatypes() {
        qmlRegisterUncreatableType<presentation::GraphViewModel>(
            "KovaaksStatsViewer", 1, 0, "GraphViewModel", "Enums only");
        qmlRegisterUncreatableType<presentation::PlaytimeGraphViewModel>(
            "KovaaksStatsViewer", 1, 0, "PlaytimeGraphViewModel", "Created in C++");
        qmlRegisterType<presentation::GraphCanvas>(
            "KovaaksStatsViewer", 1, 0, "GraphCanvas");
    }
}

#endif //KOVAAKSSTATSVIEWER_QML_REGISTRATION_H
