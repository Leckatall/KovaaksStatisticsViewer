
find_package(Protobuf REQUIRED)

set(_ksv_protoc_executable protobuf::protoc)
if(WIN32 AND Protobuf_PROTOC_EXECUTABLE)
    # protoc is a MinGW executable whose runtime DLLs live outside tools/protobuf in a vcpkg install.
    # Stage the complete runtime beside a build-local copy instead of mutating the dependency install.
    set(_ksv_protoc_runtime_dir "${CMAKE_CURRENT_BINARY_DIR}/protoc-runtime")
    file(MAKE_DIRECTORY "${_ksv_protoc_runtime_dir}")
    get_filename_component(_protoc_dir "${Protobuf_PROTOC_EXECUTABLE}" DIRECTORY)
    get_filename_component(_vcpkg_triplet_bin_dir "${_protoc_dir}/../../bin" ABSOLUTE)
    get_filename_component(_mingw_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)

    set(_ksv_protoc_runtime_files
        "${Protobuf_PROTOC_EXECUTABLE}"
        "${_vcpkg_triplet_bin_dir}/libabseil_dll.dll"
        "${_vcpkg_triplet_bin_dir}/libprotobuf-lite.dll"
        "${_vcpkg_triplet_bin_dir}/libprotobuf.dll"
        "${_vcpkg_triplet_bin_dir}/libprotoc.dll"
        "${_mingw_bin_dir}/libgcc_s_seh-1.dll"
        "${_mingw_bin_dir}/libstdc++-6.dll"
        "${_mingw_bin_dir}/libwinpthread-1.dll"
    )
    foreach(_runtime_file IN LISTS _ksv_protoc_runtime_files)
        if(NOT EXISTS "${_runtime_file}")
            message(FATAL_ERROR "Required protoc runtime file not found: ${_runtime_file}")
        endif()
        get_filename_component(_runtime_name "${_runtime_file}" NAME)
        file(COPY_FILE "${_runtime_file}" "${_ksv_protoc_runtime_dir}/${_runtime_name}" ONLY_IF_DIFFERENT)
    endforeach()
    set(_ksv_protoc_executable "${_ksv_protoc_runtime_dir}/protoc.exe")
endif()

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
        profile_builder.cpp
        profile_builder.h
        profile_service.cpp
        profile_service.h
        run_ingestor.cpp
        run_ingestor.h

        interfaces/i_proto_decoder.h
        interfaces/i_run_ingestor.h
        interfaces/i_stats_csv_parser.h
        interfaces/i_profile_migrator.h
        interfaces/i_profile_service.h
        interfaces/i_profile_serializer.h
        interfaces/i_settings_service.h
        interfaces/i_series_config_store.h
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
        PROTOC_EXE "${_ksv_protoc_executable}"
        PROTOS ${PROTO_FILES})


target_include_directories(ksv_data PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)
