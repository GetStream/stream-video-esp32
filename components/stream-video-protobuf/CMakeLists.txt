# Protobuf component - generates C code from .proto files using nanopb
# Note: This requires protoc and nanopb generator to be available
# The generated files will be in the build directory

# Proto files directory
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../stream-video/proto")
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")

idf_component_register(
    SRCS
        "${GENERATED_DIR}/signaling.pb.c"
        "${GENERATED_DIR}/sfu.pb.c"
        "${GENERATED_DIR}/sfu_signal.pb.c"
    INCLUDE_DIRS
        "${GENERATED_DIR}"
    PRIV_REQUIRES
        nanopb
)
