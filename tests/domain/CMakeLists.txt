find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(domain_tests
    domain_test.cpp
)

target_link_libraries(domain_tests PRIVATE
    ksv_domain
    GTest::gtest_main
)

gtest_discover_tests(domain_tests)
