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

target_compile_definitions(proto_decoder_tests PRIVATE
    TEST_FILES_DIR="${CMAKE_CURRENT_BINARY_DIR}/examples"
)

gtest_discover_tests(proto_decoder_tests)

add_executable(profile_service_tests
    profile_service_test.cpp
)

target_link_libraries(profile_service_tests PRIVATE
    ksv_data
    GTest::gtest_main
)

gtest_discover_tests(profile_service_tests)
