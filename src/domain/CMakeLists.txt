add_library(ksv_domain STATIC
    scenario_perf.cpp
    scenario_perf.h)

target_include_directories(ksv_domain PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)