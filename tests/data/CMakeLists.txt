find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(proto_decoder_tests
    proto_decoder_test.cpp
)

target_link_libraries(proto_decoder_tests PRIVATE
    ksv_data
    GTest::gtest_main
)
set(TEST_FILES_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../examples"
)

configure_file(
    "${TEST_FILES_DIR}/1wall6targets TE.perf"
    "${CMAKE_CURRENT_BINARY_DIR}/examples/1wall6targets TE.perf"
    COPYONLY
)

configure_file(
    "${TEST_FILES_DIR}/1wall6targets TE.csv"
    "${CMAKE_CURRENT_BINARY_DIR}/examples/1wall6targets TE.csv"
    COPYONLY
)

target_compile_definitions(proto_decoder_tests PRIVATE
    TEST_FILES_DIR="${CMAKE_CURRENT_BINARY_DIR}/examples"
)

ksv_copy_mingw_runtime_dlls(proto_decoder_tests)
gtest_discover_tests(proto_decoder_tests)

add_executable(stats_csv_parser_tests
    stats_csv_parser_test.cpp
)

target_link_libraries(stats_csv_parser_tests PRIVATE
    ksv_data
    GTest::gtest_main
)

target_compile_definitions(stats_csv_parser_tests PRIVATE
    TEST_FILES_DIR="${CMAKE_CURRENT_BINARY_DIR}/examples"
)

ksv_copy_mingw_runtime_dlls(stats_csv_parser_tests)
gtest_discover_tests(stats_csv_parser_tests)

add_executable(run_filename_tests
    run_filename_test.cpp
)

target_link_libraries(run_filename_tests PRIVATE
    ksv_data
    GTest::gtest_main
)

ksv_copy_mingw_runtime_dlls(run_filename_tests)
gtest_discover_tests(run_filename_tests)

add_executable(run_ingestor_tests
    run_ingestor_test.cpp
)

target_link_libraries(run_ingestor_tests PRIVATE
    tests_support
    ksv_data
    GTest::gtest_main
)

ksv_copy_mingw_runtime_dlls(run_ingestor_tests)
gtest_discover_tests(run_ingestor_tests)

add_executable(profile_service_tests
    profile_service_test.cpp
)

target_link_libraries(profile_service_tests PRIVATE
    tests_support
    ksv_data
    GTest::gtest_main
)

ksv_copy_mingw_runtime_dlls(profile_service_tests)
gtest_discover_tests(profile_service_tests)

add_executable(profile_builder_tests
    profile_builder_test.cpp
)

target_link_libraries(profile_builder_tests PRIVATE
    tests_support
    ksv_data
    GTest::gtest_main
)

ksv_copy_mingw_runtime_dlls(profile_builder_tests)
gtest_discover_tests(profile_builder_tests)

add_executable(profile_serializer_tests
    profile_serializer_test.cpp
)

target_link_libraries(profile_serializer_tests PRIVATE
    tests_support_domain
    ksv_data
    GTest::gtest_main
)

ksv_copy_mingw_runtime_dlls(profile_serializer_tests)
gtest_discover_tests(profile_serializer_tests)

add_executable(profile_v3_migrator_tests
    profile_v3_migrator_test.cpp
)

target_link_libraries(profile_v3_migrator_tests PRIVATE
    tests_support
    ksv_data
    GTest::gtest_main
)

target_compile_definitions(profile_v3_migrator_tests PRIVATE
    TEST_FILES_DIR="${CMAKE_CURRENT_BINARY_DIR}/examples"
)

ksv_copy_mingw_runtime_dlls(profile_v3_migrator_tests)
gtest_discover_tests(profile_v3_migrator_tests)
