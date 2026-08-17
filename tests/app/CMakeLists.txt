find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(app_tests
    qt_test_main.cpp
    completion_history_use_case_test.cpp
    graph_column_test.cpp
    graph_column_preferences_test.cpp
    series_config_test.cpp
    graph_use_case_test.cpp
    perf_column_builder_test.cpp
    playtime_graph_use_case_test.cpp
    scenario_browser_use_case_test.cpp
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
