# Developer measuring tool for the capture resource limits. Built only with -DKSV_BUILD_CAPTURE_MEASURE=ON
# (scripts/package-release.ps1 -CaptureMeasure); it is never part of a packaged build.
qt_add_executable(ksv_capture_measure
        capture_measure_main.cpp
)

# The Theme singleton is loaded from the source tree, as tests/ui/capture/capture_samples_test.cpp does.
# table_capture_fixtures.h is header-only, so it is included from the tests directory without linking tests.
target_compile_definitions(ksv_capture_measure PRIVATE KSV_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_include_directories(ksv_capture_measure PRIVATE ${CMAKE_SOURCE_DIR}/tests/ui/capture)

target_link_libraries(ksv_capture_measure PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        ksv_app
        ksv_ui
        ksv_data
        ksv_qt_data
        ksv_domain
        psapi
)
