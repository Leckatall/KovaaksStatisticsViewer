
#target_link_libraries(ksv_ui PUBLIC
#        Qt6::Core
#        Qt6::Gui
#)
#

qt_policy(SET QTP0004 NEW)
qt_policy(SET QTP0001 NEW)

add_library(ksv_ui STATIC)

target_link_libraries(ksv_ui PUBLIC ksv_contracts)
target_link_libraries(ksv_ui PRIVATE Qt6::Core Qt6::Quick Qt6::Qml)

set_source_files_properties(qml/common/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(ksv_ui
        URI KovaaksStatsViewer
        VERSION 1.0
        NO_PLUGIN
        OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/KovaaksStatsViewer
        QML_FILES
            qml/Main.qml
            qml/scenario/ScenarioWorkspace.qml
            qml/benchmark/BenchmarkWorkspace.qml
            qml/benchmark/BenchmarkIncompleteBody.qml
            qml/benchmark/BenchmarkTrackableBody.qml
            qml/benchmark/BenchmarkStatusSummary.qml
            qml/benchmark/BenchmarkHistoryCard.qml
            qml/graph/ChartTitle.qml
            qml/graph/GraphGear.qml
            qml/graph/GraphSettingsEditor.qml
            qml/graph/GraphSettingsGroup.qml
            qml/graph/GraphNumberField.qml
            qml/graph/GraphTextInput.qml
            qml/graph/GraphCatalog.js
            qml/benchmark/BenchmarkProgressGrid.qml
            qml/benchmark/TierBar.qml
            qml/graph/VisualSettingsManager.qml
            qml/graph/DashboardGraphCanvas.qml
            qml/graph/PlaytimeGraphPanel.qml
            qml/graph/ScenarioHistoryPanel.qml
            qml/graph/GraphCanvasWithTooltip.qml
            qml/graph/YAxisTitle.qml
            qml/graph/ControlPanel.qml
            qml/scenario/SelectionPanel.qml
            qml/scenario/ScenarioSearchPanel.qml
            qml/scenario/RunListView.qml
            qml/scenario/SortControls.qml
            qml/scenario/ScenarioRunItem.qml
            qml/common/CreationField.qml
            qml/common/GridCellBorders.qml
            qml/common/GridHeaderCell.qml
            qml/common/ListScrollBar.qml
            qml/common/OverflowScrollView.qml
            qml/common/Theme.qml
            qml/common/ToolbarButton.qml
            qml/common/TooltipToolButton.qml
            qml/shell/AppMenuBar.qml
            qml/shell/SettingsDialog.qml
            qml/benchmark/BenchmarkManagerDialog.qml
            qml/benchmark/BenchmarkEditorTable.qml
            qml/benchmark/BenchmarkMappingPanel.qml
            qml/benchmark/BenchmarkLibraryPane.qml
            qml/benchmark/BenchmarkEditorToolbar.qml
            qml/benchmark/BenchmarkCellEditor.qml
            qml/benchmark/TableSwatch.qml
            qml/benchmark/BenchmarkRankHeaderCell.qml
            qml/benchmark/BenchmarkEmptyGroupList.qml
            qml/benchmark/ProgressGridRow.qml
            qml/benchmark/ProgressGridGroupCell.qml
            qml/series/SeriesConfigDraftPanel.qml
            qml/series/SeriesConfigEditorDelegate.qml
            qml/series/ExpressionTreeEditor.qml
            qml/series/FieldEditors.qml
            qml/series/PathCard.qml
            qml/series/FoldedChip.qml
            qml/series/KindChooser.qml
            qml/series/ExpressionEditorDialog.qml
            qml/shell/AboutDialog.qml
            qml/shell/DirectoryPickerRow.qml
            qml/shell/FirstRunBanner.qml
            qml/shell/FirstRunWelcome.qml
        RESOURCES
            qml/icons/copy.svg
            qml/icons/clipboard-paste.svg
            qml/icons/trash-2.svg
            qml/icons/square-function.svg
            qml/icons/refresh-cw.svg
            qml/icons/folder-open.svg
            qml/icons/panel-left-close.svg
            qml/icons/panel-left-open.svg
            qml/icons/undo-2.svg
            qml/icons/save.svg
            qml/icons/square-pen.svg
            qml/icons/settings.svg
        SOURCES
            presentation/axis.h
            presentation/value_axis.h
            presentation/value_axis.cpp
            presentation/date_time_axis.h
            presentation/date_time_axis.cpp
            presentation/value_transform.h
            presentation/series_model.h
            presentation/series_model.cpp
            presentation/series_style.h
            presentation/axis_policy.h
            presentation/axis_policy.cpp
            presentation/graph_vm_base.h
            presentation/graph_vm.h
            presentation/graph_vm.cpp
            presentation/series_expression_editor_model.h
            presentation/series_expression_editor_model.cpp
            presentation/editable_expression_node.h
            presentation/editable_expression_node.cpp
            presentation/playtime_graph_vm.h
            presentation/playtime_graph_vm.cpp
            presentation/completion_history_vm.h
            presentation/completion_history_vm.cpp
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
            presentation/benchmark_issue_text.h
            presentation/benchmark_issue_text.cpp
            presentation/benchmark_format.h
            presentation/benchmark_format.cpp
            presentation/benchmark_manager_vm.h
            presentation/benchmark_manager_vm.cpp
            presentation/benchmark_table_model.h
            presentation/benchmark_table_model.cpp
            presentation/benchmark_cell_input.h
            presentation/benchmark_cell_input.cpp
            presentation/benchmark_clipboard_matrix.h
            presentation/benchmark_clipboard_matrix.cpp
            presentation/benchmark_clipboard_adapter.h
            presentation/benchmark_clipboard_adapter.cpp
            presentation/benchmark_history_vm.h
            presentation/benchmark_history_vm.cpp
            presentation/benchmark_progress_grid_model.h
            presentation/benchmark_progress_grid_model.cpp
            presentation/benchmark_tracking_vm.h
            presentation/benchmark_tracking_vm.cpp
            presentation/scenario_list_model.h
            presentation/scenario_list_model.cpp
            presentation/run_list_model.h
            presentation/run_list_model.cpp
            presentation/scenario_browser_vm.h
            presentation/scenario_browser_vm.cpp
        DEPENDENCIES QtCore
)

#set(QML_IMPORT_PATH ${CMAKE_CURRENT_BINARY_DIR}/KovaaksStatsViewer)

target_include_directories(ksv_ui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/presentation
        ${CMAKE_CURRENT_SOURCE_DIR}/components
)
