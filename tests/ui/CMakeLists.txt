find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ui_tests
    qt_test_main.cpp
    axis_model_test.cpp
    value_transform_test.cpp
    series_model_test.cpp
    graph_vm_test.cpp
    playtime_graph_vm_test.cpp
    completion_history_vm_test.cpp
    editable_expression_node_test.cpp
    session_vm_test.cpp
    settings_vm_test.cpp
    series_expression_editor_model_test.cpp
    monotone_spline_test.cpp
    scenario_browser_vm_test.cpp
)

target_link_libraries(ui_tests PRIVATE
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

ksv_copy_mingw_runtime_dlls(ui_tests)
gtest_discover_tests(ui_tests)

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
