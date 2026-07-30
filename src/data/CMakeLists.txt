
find_package(Protobuf REQUIRED)

add_library(ksv_data STATIC
        ${PROTO_SRCVS}
        ${PROTO_HDRS}
        formats/protobuf/proto_decoder.cpp
        formats/protobuf/proto_decoder.h
        interfaces/i_proto_decoder.h
)

set(PROTO_FILES ${PROJECT_SOURCE_DIR}/src/data/formats/protobuf/schema/perf.proto)

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