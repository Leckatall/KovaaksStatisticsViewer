find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ui_tests
    qt_test_main.cpp
    graph_vm_test.cpp
    session_vm_test.cpp
    settings_vm_test.cpp
    monotone_spline_test.cpp
)

target_link_libraries(ui_tests PRIVATE
    ksv_ui
    ksv_app
    ksv_data
    ksv_domain
    Qt6::Core
    Qt6::Quick
    Qt6::Qml
    Qt6::Graphs
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
    Qt6::QuickTest
    Qt6::Quick
    Qt6::Qml
)

# Register each tst_*.qml file as its own CTest test (instead of one
# add_test running the whole -input directory) so a failure in one
# component's tests doesn't hide the others from `ctest`.
#
# QT_QUICK_BACKEND=software forces the raster Quick renderer instead of the
# default RHI/Direct3D11 backend. None of these tests render GraphsView, and
# without it, running several TestCase windows in one process under
# QT_QPA_PLATFORM=offscreen intermittently fails with "the handle is
# invalid" from the D3D11 device being torn down and recreated per window
# (Windows Debug Qt builds only).
#
# QT_QUICK_CONTROLS_STYLE=Fusion avoids the platform's native Windows style,
# which doesn't support customizing controls (background, etc.) and floods
# the log with "current style does not support customization" QML warnings
# plus "OpenThemeData() failed ... the handle is invalid" warnings (the
# native style's theming API doesn't work under QT_QPA_PLATFORM=offscreen).
# Fusion supports customization and needs no native theme handle.
#
# -o <file>,txt makes QtQuickTest write its report through Qt's file logger
# (QFile-backed) instead of stdout. On this Windows/MinGW/Qt 6.11.1 setup,
# ui_qml_tests' stdout is silently dropped whenever it's redirected (which
# is always, under ctest/CLion) - confirmed by comparing a raw file
# redirection of stdout (empty, even on failure) against this -o file
# (complete FAIL!/message/location report every time). So don't rely on
# ctest/CLion's captured console output for these tests; open the log file
# instead - see the test_verbose target below.
set(qml_test_log_dir ${CMAKE_CURRENT_BINARY_DIR}/qml-test-logs)
file(MAKE_DIRECTORY ${qml_test_log_dir})

file(GLOB ui_qml_test_files ${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_*.qml)
foreach(qml_test_file ${ui_qml_test_files})
    get_filename_component(qml_test_name ${qml_test_file} NAME_WE)
    add_test(NAME ${qml_test_name}
        COMMAND ui_qml_tests -input ${qml_test_file}
            -o ${qml_test_log_dir}/${qml_test_name}.txt,txt
    )
    set_tests_properties(${qml_test_name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion"
    )
endforeach()

# `ctest` hides a failing test's output unless run with --output-on-failure,
# and even with that flag, the QML tests' own console output is unreliable
# here (see comment above) - ctest/CLion will typically show an empty or
# near-empty report for a failed tst_*.qml test regardless. The full
# FAIL!/message/location detail is always in
# <build>/tests/ui/qml-test-logs/<test-name>.txt after a run.
# `cmake --build <dir> --target test_verbose` runs everything with
# --output-on-failure for the C++ (GoogleTest) suites, where it does work.
add_custom_target(test_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
