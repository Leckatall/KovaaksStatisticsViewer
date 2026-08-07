find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(app_tests
    qt_test_main.cpp
    graph_use_case_test.cpp
    session_controller_test.cpp
)

target_link_libraries(app_tests PRIVATE
    ksv_app
    ksv_data
    ksv_domain
    Qt6::Core
    Qt6::Test
    GTest::gtest
)

ksv_copy_mingw_runtime_dlls(app_tests)
gtest_discover_tests(app_tests)
