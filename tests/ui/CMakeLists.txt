find_package(GTest CONFIG REQUIRED)

add_executable(ui_tests
    ../support/qt_gtest_main.cpp
    graphs/common/axes/value_axis_test.cpp
    graphs/common/axes/axis_policy_test.cpp
    graphs/common/axes/date_time_axis_test.cpp
    graphs/common/axes/value_transform_test.cpp
    graphs/common/series/series_model_test.cpp
    graphs/graph_vm_test.cpp
    graphs/playtime_graph_vm_test.cpp
    graphs/completion_history_vm_test.cpp
    expression/editable_expression_node_test.cpp
    app/session_vm_test.cpp
    app/settings_vm_test.cpp
    app/graph_draft_test.cpp
    expression/series_expression_editor_model_test.cpp
    graphs/common/series/monotone_spline_test.cpp
    series_painter_test.cpp
    scenarios/scenario_browser_vm_test.cpp
    benchmarks/manager/benchmark_issue_text_test.cpp
    benchmarks/manager/benchmark_manager_vm_test.cpp
    benchmarks/tracking/benchmark_tracking_vm_test.cpp
    benchmarks/tracking/benchmark_history_vm_test.cpp
    benchmarks/tracking/benchmark_progress_grid_model_test.cpp
    benchmarks/manager/benchmark_table_contract_test.cpp
    benchmarks/manager/benchmark_cell_input_test.cpp
    benchmarks/manager/benchmark_clipboard_matrix_test.cpp
    benchmarks/manager/benchmark_table_workload_test.cpp
)

target_link_libraries(ui_tests PRIVATE
    tests_support
    ksv_ui
    ksv_app
    ksv_data
    ksv_domain
    Qt6::Core
    Qt6::Quick
    Qt6::Qml
    Qt6::Test
    GTest::gtest
)

target_compile_definitions(ui_tests PRIVATE KSV_GTEST_GUI_APP)
target_precompile_headers(ui_tests PRIVATE
    <gtest/gtest.h>
    <QColor>
    <QDateTime>
    <QLocale>
    <QObject>
    <QSignalSpy>
    <QString>
    <QVariant>
    <QVariantMap>
)

# The workload measurement reads process private memory (GetProcessMemoryInfo).
if(WIN32)
    target_link_libraries(ui_tests PRIVATE psapi)
endif()
ksv_copy_mingw_runtime_dlls(ui_tests)
ksv_add_gtest_test(ui_tests)

# Qt Quick Test suite: exercises the actual QML component files (menu
# wiring, button clicks, checkbox bindings) in qml/tst_*.qml, as opposed to
# ui_tests above which only covers the plain-C++ view-model logic behind them.
add_executable(ui_qml_tests qml/tst_ui_qml.cpp)

target_link_libraries(ui_qml_tests PRIVATE
    ksv_app
    Qt6::QuickTest
    Qt6::Quick
    Qt6::Qml
)

ksv_copy_mingw_runtime_dlls(ui_qml_tests)

# Register each tst_*.qml file as its own CTest test (instead of one
# add_test running the whole -input directory) so a failure in one
# component's tests doesn't hide the others from `ctest`.

set(qml_test_log_dir ${CMAKE_CURRENT_BINARY_DIR}/qml-test-logs)
file(MAKE_DIRECTORY ${qml_test_log_dir})

file(GLOB ui_qml_test_files ${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_*.qml)
foreach(qml_test_file ${ui_qml_test_files})
    get_filename_component(qml_test_name ${qml_test_file} NAME_WE)
    add_test(NAME ${qml_test_name}
        COMMAND ui_qml_tests -input ${qml_test_file} -silent
            -o ${qml_test_log_dir}/${qml_test_name}.txt,txt
    )
    set_tests_properties(${qml_test_name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion"
    )
endforeach()
