# Headers with no Qt or ksv_data dependency, so domain_tests can use them without
# picking up the MinGW runtime-DLL requirement that linking anything above
# ksv_domain would bring (see ksv_copy_mingw_runtime_dlls in the top-level file).
add_library(tests_support_domain INTERFACE)

target_sources(tests_support_domain INTERFACE
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
        run_builders.h
        cerr_capture.h
        profile_store_files.h
)

target_link_libraries(tests_support_domain INTERFACE
        ksv_domain
)

add_library(tests_support INTERFACE)

target_sources(tests_support INTERFACE
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
        fake_settings_service.h
        fake_file_service.h
        fake_profile_service.h
        fake_session_controller.h
        fake_series_config_store.h
        kovaaks_dir.h
)

target_link_libraries(tests_support INTERFACE
        tests_support_domain
        ksv_data
        Qt6::Core
)
