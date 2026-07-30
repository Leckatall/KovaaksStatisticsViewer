
add_library(ksv_ui STATIC
        components/main/menu_bar.h)

target_link_libraries(ksv_ui PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
        Qt6::Svg
)

target_include_directories(ksv_ui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)