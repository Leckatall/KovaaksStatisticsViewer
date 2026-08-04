find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ui_tests
    qt_test_main.cpp
    graph_vm_test.cpp
    session_vm_test.cpp
    settings_vm_test.cpp
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

# Qt Quick Test suite: exercises the actual QML component files (menu
# wiring, button clicks, checkbox bindings) in qml/tst_*.qml, as opposed to
# ui_tests above which only covers the plain-C++ view-model logic behind them.
add_executable(ui_qml_tests qml/tst_ui_qml.cpp)

target_link_libraries(ui_qml_tests PRIVATE
    Qt6::QuickTest
    Qt6::Quick
    Qt6::Qml
)

add_test(NAME ui_qml_tests
    COMMAND ui_qml_tests -input ${CMAKE_CURRENT_SOURCE_DIR}/qml
)
set_tests_properties(ui_qml_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
