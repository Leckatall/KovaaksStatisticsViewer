
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
            qml/benchmark/BenchmarkPicker.qml
            qml/benchmark/BenchmarkHistoryCard.qml
            qml/graph/ChartTitle.qml
            qml/graph/GraphGear.qml
            qml/graph/GraphLineStyleControls.qml
            qml/graph/GraphRangeHint.qml
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
            qml/common/ChromeIconButton.qml
            qml/shell/AppMenuBar.qml
            qml/shell/SettingsDialog.qml
            qml/shell/ProfileStatusBar.qml
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
            qml/shell/InvalidKovaaksDirBanner.qml
            qml/shell/FirstRunWelcome.qml
        RESOURCES
            qml/icons/copy.svg
            qml/icons/clipboard-paste.svg
            qml/icons/trash-2.svg
            qml/icons/ellipsis.svg
            qml/icons/square-function.svg
            qml/icons/refresh-cw.svg
            qml/icons/folder-open.svg
            qml/icons/panel-left-close.svg
            qml/icons/panel-left-open.svg
            qml/icons/undo-2.svg
            qml/icons/save.svg
            qml/icons/square-pen.svg
            qml/icons/settings.svg
            qml/icons/x.svg
        SOURCES
            presentation/graphs/common/axes/axis.h
            presentation/graphs/common/axes/value_axis.h
            presentation/graphs/common/axes/value_axis.cpp
            presentation/graphs/common/axes/date_time_axis.h
            presentation/graphs/common/axes/date_time_axis.cpp
            presentation/graphs/common/axes/value_transform.h
            presentation/graphs/common/series/series_model.h
            presentation/graphs/common/series/series_model.cpp
            presentation/graphs/common/series/series_style.h
            presentation/graphs/common/axes/axis_policy.h
            presentation/graphs/common/axes/axis_policy.cpp
            presentation/graphs/common/graph_vm_base.h
            presentation/graphs/graph_vm.h
            presentation/graphs/graph_vm.cpp
            presentation/expression/series_expression_editor_model.h
            presentation/expression/series_expression_editor_model.cpp
            presentation/expression/editable_expression_node.h
            presentation/expression/editable_expression_node.cpp
            presentation/graphs/playtime_graph_vm.h
            presentation/graphs/playtime_graph_vm.cpp
            presentation/graphs/completion_history_vm.h
            presentation/graphs/completion_history_vm.cpp
        components/graph_canvas.h
        components/graph_canvas.cpp
        components/axis_painter.h
        components/axis_painter.cpp
        components/series_painter.h
        components/series_painter.cpp
            presentation/graphs/common/series/monotone_spline.h
            presentation/app/session_vm.h
            presentation/app/session_vm.cpp
            presentation/app/settings_vm.h
            presentation/app/settings_vm.cpp
            presentation/benchmarks/manager/benchmark_issue_text.h
            presentation/benchmarks/manager/benchmark_issue_text.cpp
            presentation/benchmarks/benchmark_format.h
            presentation/benchmarks/benchmark_format.cpp
            presentation/benchmarks/manager/benchmark_manager_vm.h
            presentation/benchmarks/manager/benchmark_manager_vm.cpp
            presentation/benchmarks/manager/benchmark_table_model.h
            presentation/benchmarks/manager/benchmark_table_model.cpp
            presentation/benchmarks/manager/benchmark_cell_input.h
            presentation/benchmarks/manager/benchmark_cell_input.cpp
            presentation/benchmarks/manager/benchmark_clipboard_matrix.h
            presentation/benchmarks/manager/benchmark_clipboard_matrix.cpp
            presentation/benchmarks/manager/benchmark_clipboard_adapter.h
            presentation/benchmarks/manager/benchmark_clipboard_adapter.cpp
            presentation/benchmarks/tracking/benchmark_history_vm.h
            presentation/benchmarks/tracking/benchmark_history_vm.cpp
            presentation/benchmarks/tracking/benchmark_progress_grid_model.h
            presentation/benchmarks/tracking/benchmark_progress_grid_model.cpp
            presentation/benchmarks/tracking/benchmark_tracking_vm.h
            presentation/benchmarks/tracking/benchmark_tracking_vm.cpp
            presentation/scenarios/scenario_list_model.h
            presentation/scenarios/scenario_list_model.cpp
            presentation/scenarios/run_list_model.h
            presentation/scenarios/run_list_model.cpp
            presentation/scenarios/scenario_browser_vm.h
            presentation/scenarios/scenario_browser_vm.cpp
        DEPENDENCIES QtCore
)

#set(QML_IMPORT_PATH ${CMAKE_CURRENT_BINARY_DIR}/KovaaksStatsViewer)

target_include_directories(ksv_ui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/components
)

# qmltyperegistrar includes each QML_ELEMENT header by bare filename behind __has_include, so a header
# missing from the include path drops its type from registration without any diagnostic. Every directory
# holding a QML_ELEMENT header must be listed here.
target_include_directories(ksv_ui PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/presentation/benchmarks/manager
        ${CMAKE_CURRENT_SOURCE_DIR}/presentation/expression
        ${CMAKE_CURRENT_SOURCE_DIR}/presentation/scenarios
)
