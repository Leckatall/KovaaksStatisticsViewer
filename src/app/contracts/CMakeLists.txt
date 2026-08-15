add_library(ksv_contracts INTERFACE)

target_sources(ksv_contracts INTERFACE
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
            completion_history.h
            scenario_summary.h
            run_performance.h
            graph_column.h
            graph_series.h
            i_graph_use_case.h
            i_playtime_graph_use_case.h
            i_completion_history_use_case.h
            i_graph_column_preferences.h
)

target_include_directories(ksv_contracts INTERFACE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/app
)

target_link_libraries(ksv_contracts INTERFACE ksv_domain)

set_target_properties(ksv_contracts PROPERTIES VERIFY_INTERFACE_HEADER_SETS ON)
