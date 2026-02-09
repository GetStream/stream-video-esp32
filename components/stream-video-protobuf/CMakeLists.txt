# Protobuf component - generates C code from .proto files using nanopb
# Note: This requires protoc and nanopb generator to be available
# The generated files will be in the build directory

# Proto files directory
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../stream-video/proto")
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")

find_program(PYTHON_EXECUTABLE python3)
find_program(PROTOC_EXECUTABLE protoc)

if(DEFINED ENV{NANOPB_GENERATOR})
    set(NANOPB_GENERATOR "$ENV{NANOPB_GENERATOR}")
elseif(PYTHON_EXECUTABLE)
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} -c "import nanopb, os; print(os.path.join(os.path.dirname(nanopb.__file__), 'generator', 'protoc-gen-nanopb'))"
        OUTPUT_VARIABLE NANOPB_GENERATOR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

set(PROTO_FILES
    "${PROTO_DIR}/signaling.proto"
    "${PROTO_DIR}/sfu.proto"
    "${PROTO_DIR}/models.proto"
    "${PROTO_DIR}/sfu_signal.proto"
)

set(GENERATED_FILES
    "${GENERATED_DIR}/signaling.pb.c"
    "${GENERATED_DIR}/signaling.pb.h"
    "${GENERATED_DIR}/sfu.pb.c"
    "${GENERATED_DIR}/sfu.pb.h"
    "${GENERATED_DIR}/models.pb.c"
    "${GENERATED_DIR}/models.pb.h"
    "${GENERATED_DIR}/sfu_signal.pb.c"
    "${GENERATED_DIR}/sfu_signal.pb.h"
)

idf_component_register(
    SRCS
        "${GENERATED_DIR}/signaling.pb.c"
        "${GENERATED_DIR}/sfu.pb.c"
        "${GENERATED_DIR}/sfu_signal.pb.c"
        "${GENERATED_DIR}/models.pb.c"
    INCLUDE_DIRS
        "${GENERATED_DIR}"
    PRIV_REQUIRES
        nanopb
)

# add_custom_command/target are not available in CMake script mode
# (used by ESP-IDF's component_get_requirements phase)
if(NOT CMAKE_SCRIPT_MODE_FILE)
    if(PROTOC_EXECUTABLE AND NANOPB_GENERATOR)
        add_custom_command(
            OUTPUT ${GENERATED_FILES}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}"
            COMMAND ${PROTOC_EXECUTABLE}
                --plugin=protoc-gen-nanopb=${NANOPB_GENERATOR}
                --nanopb_out="${GENERATED_DIR}"
                --proto_path="${PROTO_DIR}"
                ${PROTO_FILES}
            DEPENDS ${PROTO_FILES}
            COMMENT "Generating nanopb sources"
        )
        add_custom_target(stream_video_nanopb_gen DEPENDS ${GENERATED_FILES})
        add_dependencies(${COMPONENT_LIB} stream_video_nanopb_gen)
    else()
        message(WARNING "nanopb generator not found; using existing generated sources")
    endif()
endif()
