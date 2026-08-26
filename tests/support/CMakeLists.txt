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
)

target_link_libraries(tests_support INTERFACE
        ksv_data
        Qt6::Core
)
