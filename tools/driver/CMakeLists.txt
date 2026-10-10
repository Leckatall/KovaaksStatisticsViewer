# UI automation driver: the in-process inspector/input/screenshot core (a
# library, so the integration suite can exercise it) and the ksv_driver host
# that serves it over localhost for scripts/ui_driver.py. Dev-only; added with
# ksv_gallery under KSV_BUILD_GALLERY in non-Release builds.
add_library(ksv_driver_core STATIC
        driver_dataset.cpp
        driver_dataset.h
        driver_server.cpp
        driver_server.h
        message_log.cpp
        message_log.h
        selector.cpp
        selector.h
        ui_driver.cpp
        ui_driver.h
)
target_include_directories(ksv_driver_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ksv_driver_core PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::Network
        Qt6::Test  # QTest input synthesis (mouseClick, keySequence, qWaitFor)
        ksv_gallery_support
        ksv_qt_data
        ksv_data
        ksv_domain
)

qt_add_executable(ksv_driver
        driver_main.cpp
)

target_link_libraries(ksv_driver PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        Qt6::Svg
        ksv_driver_core
        ksv_gallery_support
        ksv_app
        ksv_ui
        ksv_data
        ksv_qt_data
        ksv_domain
)

target_compile_definitions(ksv_driver PRIVATE
        KSV_QML_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src/ui/qml"
        KSV_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/tests/examples"
)

ksv_copy_mingw_runtime_dlls(ksv_driver)
