find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(qt_data_tests
    qt_test_main.cpp
    graph_line_config_test.cpp
    settings_service_test.cpp
    file_service_test.cpp
)

target_link_libraries(qt_data_tests PRIVATE
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

target_compile_definitions(qt_data_tests PRIVATE
    TEST_FILES_DIR="${CMAKE_CURRENT_BINARY_DIR}/examples"
)

ksv_copy_mingw_runtime_dlls(qt_data_tests)
gtest_discover_tests(qt_data_tests)
