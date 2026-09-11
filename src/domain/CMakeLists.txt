add_library(ksv_domain STATIC
    run.cpp
    run.h
        source_directory.cpp
        source_directory.h
        user_profile.cpp
        user_profile.h
        benchmarks/benchmark_ids.h
        benchmarks/benchmark.h
        benchmarks/benchmark_validation.h
        benchmarks/benchmark_validation.cpp
        benchmarks/benchmark_evaluator.h
        benchmarks/benchmark_evaluator.cpp
        benchmarks/benchmark_resolution.h
        benchmarks/benchmark_projection.h
        benchmarks/benchmark_editor.h
        benchmarks/benchmark_editor.cpp
)

target_include_directories(ksv_domain PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
