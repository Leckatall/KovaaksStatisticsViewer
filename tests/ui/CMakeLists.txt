find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ui_tests
    qt_test_main.cpp
    graph_vm_test.cpp
    session_vm_test.cpp
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

gtest_discover_tests(ui_tests)
