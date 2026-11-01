
find_package(Protobuf REQUIRED)

add_library(ksv_data STATIC
        formats/csv/run_filename.cpp
        formats/csv/run_filename.h
        formats/csv/stats_csv_parser.cpp
        formats/csv/stats_csv_parser.h
        formats/protobuf/proto_decoder.cpp
        formats/protobuf/proto_decoder.h
        formats/protobuf/profile_serializer.cpp
        formats/protobuf/profile_serializer.h
        formats/protobuf/migration/profile_v3_migrator.cpp
        formats/protobuf/migration/profile_v3_migrator.h
        profile_importer.cpp
        profile_importer.h
        source_path_key.h
        interfaces/i_profile_importer.h
        profile_service.cpp
        profile_service.h
        run_ingestor.cpp
        run_ingestor.h
        benchmarks_service.cpp
        benchmarks_service.h
        graph_configuration_service.cpp
        graph_configuration_service.h

        interfaces/i_proto_decoder.h
        interfaces/i_run_ingestor.h
        interfaces/i_stats_csv_parser.h
        interfaces/i_profile_migrator.h
        interfaces/i_profile_service.h
        interfaces/i_profile_serializer.h
        interfaces/i_settings_service.h
        interfaces/i_graph_configuration_persistence.h
        interfaces/i_graph_configuration_service.h
        interfaces/i_day_source.h
        interfaces/benchmark_library_snapshot.h
        interfaces/i_benchmark_store.h
        interfaces/i_benchmarks_service.h
        interfaces/i_clipboard_image_writer.h
        interfaces/i_png_image_writer.h
)

set(PROTO_FILES
        ${PROJECT_SOURCE_DIR}/src/data/formats/protobuf/schema/perf.proto
        ${PROJECT_SOURCE_DIR}/src/data/formats/protobuf/schema/profile.proto
        ${PROJECT_SOURCE_DIR}/src/data/formats/protobuf/migration/profile_v3.proto
)

target_link_libraries(ksv_data PUBLIC
        protobuf::libprotobuf
        ksv_domain
        ksv_contracts
)

protobuf_generate(TARGET ksv_data PROTO_SRCS PROTO_HDRS
        PROTOC_EXE protobuf::protoc
        PROTOS ${PROTO_FILES})


target_include_directories(ksv_data PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)
