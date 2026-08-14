add_library(ksv_domain STATIC
    scenario_perf.cpp
    scenario_perf.h
    run_performance.h
        source_directory.cpp
        source_directory.h
        user_profile.cpp
        user_profile.h
)

target_include_directories(ksv_domain PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
