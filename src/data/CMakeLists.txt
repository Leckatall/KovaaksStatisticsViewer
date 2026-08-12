
find_package(Protobuf REQUIRED)

# vcpkg's protoc.exe (tools/protobuf/) doesn't ship next to the DLLs it needs
# to run, so invoking it from an environment that doesn't already have those
# on PATH (anything other than CLion's configured terminal) fails with
# STATUS_DLL_NOT_FOUND when protobuf_generate() runs it below. It needs two
# separate sets of DLLs: the protobuf/abseil runtime from vcpkg's own
# installed/<triplet>/bin/, and the MinGW runtime (libgcc_s_seh-1,
# libstdc++-6 -> libwinpthread-1) since it's itself a MinGW-built binary.
# Copy both alongside it once at configure time so protoc.exe is runnable
# standalone, same fix as the libwinpthread-1.dll copy for myapp.exe in the
# top-level CMakeLists.txt.
if (WIN32 AND Protobuf_PROTOC_EXECUTABLE)
    get_filename_component(_protoc_dir "${Protobuf_PROTOC_EXECUTABLE}" DIRECTORY)

    get_filename_component(_vcpkg_triplet_bin_dir "${_protoc_dir}/../../bin" ABSOLUTE)
    file(GLOB _protobuf_runtime_dlls "${_vcpkg_triplet_bin_dir}/libprotobuf*.dll" "${_vcpkg_triplet_bin_dir}/libprotoc.dll")
    if (_protobuf_runtime_dlls)
        file(COPY ${_protobuf_runtime_dlls} DESTINATION "${_protoc_dir}")
    endif()

    get_filename_component(_mingw_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_mingw_runtime_dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        if (EXISTS "${_mingw_bin_dir}/${_mingw_runtime_dll}")
            file(COPY "${_mingw_bin_dir}/${_mingw_runtime_dll}" DESTINATION "${_protoc_dir}")
        endif()
    endforeach()
endif()

add_library(ksv_data STATIC
        formats/protobuf/proto_decoder.cpp
        formats/protobuf/proto_decoder.h
        formats/protobuf/profile_serializer.cpp
        formats/protobuf/profile_serializer.h
        profile_builder.cpp
        profile_builder.h
        profile_service.cpp
        profile_service.h

        interfaces/i_proto_decoder.h
        interfaces/i_profile_service.h
        interfaces/i_profile_serializer.h
        interfaces/i_settings_service.h
)

set(PROTO_FILES
        ${PROJECT_SOURCE_DIR}/src/data/formats/protobuf/schema/perf.proto
        ${PROJECT_SOURCE_DIR}/src/data/formats/protobuf/schema/cache.proto
)

target_link_libraries(ksv_data PUBLIC
        protobuf::libprotobuf
        ksv_domain
)

protobuf_generate(TARGET ksv_data PROTO_SRCS PROTO_HDRS
        PROTOS ${PROTO_FILES})


target_include_directories(ksv_data PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)