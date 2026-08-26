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

ksv_copy_mingw_runtime_dlls(proto_decoder_tests)
gtest_discover_tests(proto_decoder_tests)

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
    ksv_data
    GTest::gtest_main
)

ksv_copy_mingw_runtime_dlls(profile_serializer_tests)
gtest_discover_tests(profile_serializer_tests)
