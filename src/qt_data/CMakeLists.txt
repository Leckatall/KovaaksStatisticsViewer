
add_library(ksv_qt_data STATIC
        file_service.cpp
        file_service.h
        ../data/interfaces/i_file_service.h
        graph_line_config.cpp
        graph_line_config.h
        ../data/interfaces/i_graph_line_config.h
        settings_service.cpp
        settings_service.h
        ../data/interfaces/i_settings_service.h
)

target_link_libraries(ksv_qt_data PUBLIC
        Qt6::Core
        ksv_contracts
        ksv_domain
        ksv_data
)
target_include_directories(ksv_qt_data PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
