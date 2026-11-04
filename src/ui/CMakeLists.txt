
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
            qml/graph/GraphHiddenNotice.qml
            qml/graph/GraphLineStyleControls.qml
            qml/graph/GraphRangeHint.qml
            qml/graph/GraphSettingsEditor.qml
            qml/graph/GraphSettingsGroup.qml
            qml/graph/GraphNumberField.qml
            qml/graph/GraphTextInput.qml
            qml/graph/GraphVisibilityMenu.qml
            qml/graph/NoGraphsShown.qml
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
            qml/common/ActionButton.qml
            qml/common/CommitOnFinishField.qml
            qml/common/CreationField.qml
            qml/common/DragHandleDots.qml
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
            qml/shell/CaptureStatusStrip.qml
            qml/shell/CapturePreviewDialog.qml
            qml/graph/CaptureActions.qml
            qml/benchmark/BenchmarkManagerDialog.qml
            qml/benchmark/BenchmarkEditorTable.qml
            qml/benchmark/BenchmarkMappingPanel.qml
            qml/benchmark/BenchmarkLibraryPane.qml
            qml/benchmark/BenchmarkEditorToolbar.qml
            qml/benchmark/BenchmarkCellEditor.qml
            qml/benchmark/TableSwatch.qml
            qml/benchmark/BenchmarkRankHeaderCell.qml
            qml/benchmark/BenchmarkGroupCommand.qml
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
            qml/series/AxisNameDialog.qml
            qml/series/AxesPanel.qml
            qml/series/axisNames.js
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
            qml/icons/check.svg
            qml/icons/camera.svg
            qml/icons/chevron-down.svg
            qml/icons/chevron-right.svg
            qml/icons/circle-alert.svg
            qml/icons/info.svg
            qml/icons/eye.svg
            qml/icons/eye-off.svg
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
        components/capture_preview_item.h
        components/capture_preview_item.cpp
        components/axis_painter.h
        components/axis_painter.cpp
        components/series_painter.h
        components/series_painter.cpp
        components/graph_scene.h
        components/graph_scene_painter.h
        components/graph_scene_painter.cpp
        capture/capture_snapshot.h
        capture/capture_snapshot.cpp
        capture/capture_appearance.h
        capture/capture_appearance.cpp
        capture/capture_coordinator.h
        capture/capture_coordinator.cpp
        capture/capture_feedback.h
        capture/capture_feedback.cpp
        capture/capture_sources.h
        capture/capture_sources.cpp
        capture/capture_font_description.h
        capture/capture_fonts.h
        capture/capture_fonts.cpp
        capture/capture_text.h
        capture/capture_text.cpp
        capture/capture_sizing.h
        capture/capture_sizing.cpp
        capture/capture_limits.h
        capture/capture_limits.cpp
        capture/capture_layout.h
        capture/capture_layout.cpp
        capture/capture_render.h
        capture/capture_render.cpp
        capture/capture_render_queue.h
        capture/capture_render_queue.cpp
        capture/capture_renderer.h
        capture/capture_renderer.cpp
        capture/graph_capture_layout.h
        capture/graph_capture_layout.cpp
        capture/table_capture_layout.h
        capture/table_capture_layout.cpp
            presentation/graphs/common/series/monotone_spline.h
            presentation/app/session_vm.h
            presentation/app/session_vm.cpp
            presentation/app/settings_vm.h
            presentation/app/settings_vm.cpp
            presentation/benchmarks/manager/benchmark_issue_text.h
            presentation/benchmarks/manager/benchmark_issue_text.cpp
            presentation/benchmarks/benchmark_format.h
            presentation/benchmarks/benchmark_format.cpp
            presentation/benchmarks/manager/benchmark_group_command.h
            presentation/benchmarks/manager/benchmark_group_command.cpp
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
            presentation/benchmarks/tracking/progress_grid_content.h
            presentation/benchmarks/tracking/benchmark_progress_grid_model.cpp
            presentation/benchmarks/tracking/benchmark_tracking_vm.h
            presentation/benchmarks/tracking/benchmark_tracking_vm.cpp
            presentation/capture/capture_vm.h
            presentation/capture/capture_vm.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/presentation/benchmarks/tracking
        ${CMAKE_CURRENT_SOURCE_DIR}/presentation/capture
        ${CMAKE_CURRENT_SOURCE_DIR}/presentation/expression
        ${CMAKE_CURRENT_SOURCE_DIR}/presentation/graphs
        ${CMAKE_CURRENT_SOURCE_DIR}/presentation/graphs/common
        ${CMAKE_CURRENT_SOURCE_DIR}/presentation/scenarios
)
