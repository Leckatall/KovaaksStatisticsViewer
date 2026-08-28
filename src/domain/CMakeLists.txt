add_library(ksv_domain STATIC
    run.cpp
    run.h
        source_directory.cpp
        source_directory.h
        user_profile.cpp
        user_profile.h
)

target_include_directories(ksv_domain PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
