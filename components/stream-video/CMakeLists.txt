idf_component_register(
    SRCS
        "src/room.c"
        "src/signaling/websocket_client.c"
        "src/signaling/signaling_protocol.c"
        "src/signaling/message_handler.c"
        "src/sfu/sfu_client.c"
        "src/sfu/sfu_protocol.c"
        "src/sfu/connection_manager.c"
        "src/webrtc/peer_connection.c"
        "src/webrtc/media_track.c"
        "src/webrtc/sdp_handler.c"
        "src/protobuf/message_serializer.c"
        "src/protobuf/message_deserializer.c"
        "src/utils/logger.c"
        "src/utils/error_handler.c"
        "src/utils/token_utils.c"
        "src/utils/http_client.c"
        "src/auth/auth_client.c"
        "src/coordinator/coordinator_client.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        stream-video-protobuf
        esp_capture
    PRIV_REQUIRES
        esp_peer
        # av_render and media_lib_os - these may be part of ESP-IDF or need manual installation
        # Uncomment when available:
        # av_render
        # media_lib_os
        nanopb
        stream-video-protobuf
        nvs_flash
        esp_http_client
        esp_websocket_client
        mbedtls
        json
        # Note: json is the ESP-IDF v5.x built-in component name for cJSON
        # For ESP-IDF v6+, use cjson from Component Manager (espressif/cjson)
)


