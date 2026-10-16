
add_library(ksv_qt_data STATIC
        file_service.cpp
        file_service.h
        ../data/interfaces/i_file_service.h
        settings_service.cpp
        settings_service.h
        ../data/interfaces/i_settings_service.h
        series_config_codec.cpp
        series_config_codec.h
        graph_configuration_codec.cpp
        graph_configuration_codec.h
        graph_configuration_persistence.cpp
        graph_configuration_persistence.h
        system_day_source.cpp
        system_day_source.h
        ../data/interfaces/i_day_source.h
        ../data/interfaces/i_graph_configuration_persistence.h
        benchmark_store.cpp
        benchmark_store.h
        ../data/interfaces/i_benchmark_store.h
        ../data/interfaces/benchmark_library_snapshot.h
        playlist_reader.cpp
        playlist_reader.h
        ../data/interfaces/i_playlist_reader.h
)

target_link_libraries(ksv_qt_data PUBLIC
        Qt6::Core
        Qt6::Gui
        ksv_contracts
        ksv_domain
        ksv_data
)
target_include_directories(ksv_qt_data PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
