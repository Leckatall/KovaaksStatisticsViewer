
add_library(ksv_qt_data STATIC
        file_service.cpp
        file_service.h
        ../data/interfaces/i_file_service.h
        settings_service.cpp
        settings_service.h
        ../data/interfaces/i_settings_service.h
        series_config_store.cpp
        series_config_store.h
        ../data/interfaces/i_series_config_store.h
        benchmark_repository.cpp
        benchmark_repository.h
        ../data/interfaces/i_benchmark_repository.h
        playlist_reader.cpp
        playlist_reader.h
        ../data/interfaces/i_playlist_reader.h
)

target_link_libraries(ksv_qt_data PUBLIC
        Qt6::Core
        ksv_contracts
        ksv_domain
        ksv_data
)
target_include_directories(ksv_qt_data PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
