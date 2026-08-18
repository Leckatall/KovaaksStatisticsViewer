add_library(ksv_contracts STATIC
        series_config.cpp
)

target_sources(ksv_contracts PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
        completion_history.h
        scenario_summary.h
        run_performance.h
        graph_column.h
        graph_series.h
        series_config.h
        i_graph_use_case.h
        i_playtime_graph_use_case.h
        i_completion_history_use_case.h
        i_graph_column_preferences.h
        i_average_line_use_case.h
)

target_include_directories(ksv_contracts PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/app
)

target_link_libraries(ksv_contracts PUBLIC ksv_domain)

set_target_properties(ksv_contracts PROPERTIES VERIFY_INTERFACE_HEADER_SETS ON)
