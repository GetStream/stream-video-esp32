set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")

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

file(MAKE_DIRECTORY "${GENERATED_DIR}")

# If generated protobuf sources are missing, try to generate at configure time
set(FIRST_GEN "${GENERATED_DIR}/signaling.pb.c")
if(NOT CMAKE_SCRIPT_MODE_FILE AND NOT EXISTS "${FIRST_GEN}")
    find_program(PROTOC_EXECUTABLE protoc)
    find_program(PYTHON_EXECUTABLE python3)
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
    if(PROTOC_EXECUTABLE AND NANOPB_GENERATOR)
        execute_process(
            COMMAND ${PROTOC_EXECUTABLE}
                --plugin=protoc-gen-nanopb=${NANOPB_GENERATOR}
                --nanopb_out="${GENERATED_DIR}"
                --proto_path="${PROTO_DIR}"
                ${PROTO_FILES}
            RESULT_VARIABLE PROTOC_RESULT
        )
        if(NOT PROTOC_RESULT EQUAL 0)
            message(FATAL_ERROR "stream-video: Generated protobuf files missing and protoc failed. Install protoc and pip install nanopb, or commit generated/")
        endif()
    else()
        message(FATAL_ERROR "stream-video: Generated protobuf files missing. Install protoc and pip install nanopb, or commit generated/")
    endif()
endif()

set(STREAM_VIDEO_SRCS
    "src/room.c"
    "src/signaling/websocket_client.c"
    "src/signaling/signaling_protocol.c"
    "src/signaling/message_handler.c"
    "src/sfu/sfu_client.c"
    "src/sfu/sfu_protocol.c"
    "src/webrtc/peer_connection.c"
    "src/webrtc/media_track.c"
    "src/webrtc/sdp_handler.c"
    "src/protobuf/message_serializer.c"
    "src/protobuf/message_deserializer.c"
    "src/capture/default_capture.c"
    "src/utils/logger.c"
    "src/utils/error_handler.c"
    "src/utils/token_utils.c"
    "src/utils/http_client.c"
    "src/auth/auth_client.c"
    "src/coordinator/coordinator_client.c"
    "${GENERATED_DIR}/signaling.pb.c"
    "${GENERATED_DIR}/sfu.pb.c"
    "${GENERATED_DIR}/sfu_signal.pb.c"
    "${GENERATED_DIR}/models.pb.c"
)
if(CONFIG_STREAM_VIDEO_UDP_STUN_TRACE)
    list(APPEND STREAM_VIDEO_SRCS "src/utils/udp_trace.c")
endif()

idf_component_register(
    SRCS
        ${STREAM_VIDEO_SRCS}
    INCLUDE_DIRS
        "include"
        "${GENERATED_DIR}"
    PRIV_REQUIRES
        esp_peer
        esp_capture
        lwip
        nanopb
        nvs_flash
        esp_http_client
        esp_websocket_client
        mbedtls
        json
        driver
        esp_timer
        esp_video_codec
        esp_audio_codec
)

# codec_board is optional — provided by the application for board-specific audio.
# Add its include path and link library when present in the build.
if(NOT CMAKE_SCRIPT_MODE_FILE)
    idf_build_get_property(build_components BUILD_COMPONENTS)
    if("codec_board" IN_LIST build_components)
        idf_component_get_property(cb_dir codec_board COMPONENT_DIR)
        idf_component_get_property(cb_lib codec_board COMPONENT_LIB)
        target_include_directories(${COMPONENT_LIB} PRIVATE "${cb_dir}/include")
        target_link_libraries(${COMPONENT_LIB} PRIVATE ${cb_lib})
    endif()
endif()

# Protobuf re-generation target (optional, only when protoc + nanopb are available)
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
        message(STATUS "nanopb generator not found; using existing generated sources")
    endif()
endif()
