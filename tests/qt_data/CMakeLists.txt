find_package(GTest CONFIG REQUIRED)

add_executable(qt_data_tests
    ../support/qt_gtest_main.cpp
    settings_service_test.cpp
    file_service_test.cpp
    graph_configuration_persistence_test.cpp
    system_day_source_test.cpp
    benchmark_store_test.cpp
    playlist_reader_test.cpp
)

target_link_libraries(qt_data_tests PRIVATE
    tests_support
    ksv_qt_data
    ksv_data
    ksv_domain
    Qt6::Core
    Qt6::Test
    GTest::gtest
)

set(TEST_FILES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../examples")

configure_file(
    "${TEST_FILES_DIR}/1wall6targets TE.perf"
    "${CMAKE_CURRENT_BINARY_DIR}/examples/1wall6targets TE.perf"
    COPYONLY
)
configure_file(
    "${TEST_FILES_DIR}/VT FlyTS Novice S5.perf"
    "${CMAKE_CURRENT_BINARY_DIR}/examples/VT FlyTS Novice S5.perf"
    COPYONLY
)
configure_file(
    "${TEST_FILES_DIR}/Viscose Benchmarks Beta - Intermediate.json"
    "${CMAKE_CURRENT_BINARY_DIR}/examples/Viscose Benchmarks Beta - Intermediate.json"
    COPYONLY
)

target_compile_definitions(qt_data_tests PRIVATE
    TEST_FILES_DIR="${CMAKE_CURRENT_BINARY_DIR}/examples"
)

target_precompile_headers(qt_data_tests PRIVATE
    <gtest/gtest.h>
    <QDir>
    <QFile>
    <QSettings>
    <QTemporaryDir>
)

ksv_copy_mingw_runtime_dlls(qt_data_tests)
ksv_add_gtest_test(qt_data_tests)
