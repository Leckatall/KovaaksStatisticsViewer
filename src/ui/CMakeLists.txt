
#target_link_libraries(ksv_ui PUBLIC
#        Qt6::Core
#        Qt6::Gui
#)
#

qt_policy(SET QTP0004 NEW)
qt_policy(SET QTP0001 NEW)

add_library(ksv_ui STATIC)

target_link_libraries(ksv_ui PRIVATE Qt6::Core Qt6::Quick Qt6::Qml)

qt_add_qml_module(ksv_ui
        URI KovaaksStatsViewer
        VERSION 1.0
        NO_PLUGIN
        OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/KovaaksStatsViewer
        QML_FILES
            qml/Main.qml
            qml/DashboardGraphCanvas.qml
            qml/PlaytimeGraphPanel.qml
            qml/GraphCanvasWithTooltip.qml
            qml/ControlPanel.qml
            qml/AppMenuBar.qml
            qml/SettingsDialog.qml
            qml/DirectoryPickerRow.qml
        SOURCES
            presentation/axis_model.h
            presentation/axis_model.cpp
            presentation/value_transform.h
            presentation/series_model.h
            presentation/series_model.cpp
            presentation/graph_vm_base.h
            presentation/graph_vm.h
            presentation/graph_vm.cpp
            presentation/playtime_graph_vm.h
            presentation/playtime_graph_vm.cpp
        components/graph_canvas.h
        components/graph_canvas.cpp
        components/axis_painter.h
        components/axis_painter.cpp
        components/series_painter.h
        components/series_painter.cpp
            presentation/monotone_spline.h
            presentation/session_vm.h
            presentation/session_vm.cpp
            presentation/settings_vm.h
            presentation/settings_vm.cpp
)

#set(QML_IMPORT_PATH ${CMAKE_CURRENT_BINARY_DIR}/KovaaksStatsViewer)

target_include_directories(ksv_ui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/presentation
        ${CMAKE_CURRENT_SOURCE_DIR}/components
)
