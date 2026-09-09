find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(domain_tests
    domain_test.cpp
    benchmark_validation_test.cpp
    benchmark_evaluator_test.cpp
    benchmark_projection_shape_test.cpp
    user_profile_benchmark_queries_test.cpp
    gallery_dataset_test.cpp
    benchmark_projection_timing_test.cpp
    ../../tools/gallery/gallery_dataset.cpp
)

target_include_directories(domain_tests PRIVATE
    ../../tools/gallery
)

target_link_libraries(domain_tests PRIVATE
    tests_support_domain
    ksv_domain
    GTest::gtest_main
)

gtest_discover_tests(domain_tests)
