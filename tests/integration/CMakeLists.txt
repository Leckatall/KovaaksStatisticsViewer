find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# Integration suite: links every layer together and drives the REAL wired
# object graph (no hand-rolled fakes) against the checked-in .perf fixtures,
# catching wiring/pipeline/QML-registration bugs the per-layer unit tests miss.
add_executable(integration_tests
    qt_test_main.cpp
    perf_pipeline_test.cpp
    composition_root_test.cpp
    profile_cascade_test.cpp
    graph_canvas_geometry_test.cpp
    axis_painter_test.cpp
    qml_smoke_test.cpp
    dashboard_ui_test.cpp
)

target_link_libraries(integration_tests PRIVATE
    ksv_ui
    ksv_app
    ksv_qt_data
    ksv_data
    ksv_domain
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Test
    GTest::gtest
)

set(TEST_FILES_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../examples"
)

foreach(fixture "1wall6targets TE.perf" "VT FlyTS Novice S5.perf")
    configure_file(
        "${TEST_FILES_DIR}/${fixture}"
        "${CMAKE_CURRENT_BINARY_DIR}/examples/${fixture}"
        COPYONLY
    )
endforeach()

target_compile_definitions(integration_tests PRIVATE
    TEST_FILES_DIR="${CMAKE_CURRENT_BINARY_DIR}/examples"
)

ksv_copy_mingw_runtime_dlls(integration_tests)

# Same offscreen/software/Fusion env the ui_qml_tests use: these tests build a
# QGuiApplication and instantiate QtQuick items (GraphCanvas) / load Main.qml.
gtest_discover_tests(integration_tests
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion"
)
