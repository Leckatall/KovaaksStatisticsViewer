add_library(ksv_contracts STATIC
        series_config.cpp
        graph_configuration.cpp
        graph_presentation.cpp
        run_series_editing.cpp
        range_projection.cpp
        playtime_view.cpp
        expression_dsl.cpp
)

target_sources(ksv_contracts PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
        completion_history.h
        scenario_summary.h
        run_performance.h
        graph_info.h
        series_config.h
        graph_configuration.h
        graph_presentation.h
        run_series_editing.h
        range_projection.h
        playtime_view.h
        expression_dsl.h
        i_graph_use_case.h
        i_playtime_graph_use_case.h
        i_completion_history_use_case.h
        i_average_line_use_case.h
        i_graph_configuration_use_case.h
        i_scenario_browser_use_case.h
        benchmark_resolution_snapshot.h
        benchmark_workspace_snapshot.h
        benchmark_manager_state.h
        benchmark_editor_seed.h
        playlist_seed.h
        i_benchmark_resolution_use_case.h
        i_benchmark_tracking_use_case.h
        i_benchmark_manager_use_case.h
        i_recent_benchmarks.h
)

target_include_directories(ksv_contracts PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/app
)

target_link_libraries(ksv_contracts PUBLIC ksv_domain)

set_target_properties(ksv_contracts PROPERTIES VERIFY_INTERFACE_HEADER_SETS ON)
