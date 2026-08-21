#ifndef KOVAAKSSTATSVIEWER_QML_REGISTRATION_H
#define KOVAAKSSTATSVIEWER_QML_REGISTRATION_H

#include <QtQml>

#include <ui/components/graph_canvas.h>
#include <presentation/completion_history_vm.h>
#include <presentation/graph_vm.h>
#include <presentation/playtime_graph_vm.h>
#include <presentation/scenario_browser_vm.h>
#include <presentation/series_model.h>
#include <presentation/series_expression_editor_model.h>

namespace ksv {
    // Referencing these types keeps the linker from dropping the static QML
    // module's auto-registration objects (ksv_ui is built NO_PLUGIN). Called
    // from every executable that loads the QML module.
    inline void declare_metatypes() {
        // TODO: I believe registering uncreatable type lines are unnecessary I will remove if this is verified with testing
        qmlRegisterUncreatableType<presentation::PlaytimeGraphViewModel>(
            "KovaaksStatsViewer", 1, 0, "PlaytimeGraphViewModel", "Created in C++");
        qmlRegisterUncreatableType<presentation::CompletionHistoryViewModel>(
            "KovaaksStatsViewer", 1, 0, "CompletionHistoryViewModel", "Created in C++");
        qmlRegisterUncreatableType<presentation::ScenarioBrowserViewModel>(
            "KovaaksStatsViewer", 1, 0, "ScenarioBrowserViewModel", "Created in C++");
        qmlRegisterUncreatableType<presentation::SeriesModel>(
            "KovaaksStatsViewer", 1, 0, "SeriesModel", "Created in C++");
        qmlRegisterUncreatableType<presentation::SeriesExpressionEditorModel>(
            "KovaaksStatsViewer", 1, 0, "SeriesExpressionEditorModel", "Created in C++");
        qmlRegisterType<ui::GraphCanvas>(
            "KovaaksStatsViewer", 1, 0, "GraphCanvas");
    }
}

#endif //KOVAAKSSTATSVIEWER_QML_REGISTRATION_H
