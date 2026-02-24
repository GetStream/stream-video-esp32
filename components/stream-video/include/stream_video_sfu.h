/**
 * @file stream_video_sfu.h
 * @brief SFU WebSocket client for media signaling
 * 
 * Handles connection to Stream's SFU server for WebRTC media signaling.
 * SFU WebSocket URL is obtained from coordinator's joinCall response.
 */

#ifndef STREAM_VIDEO_SFU_H
#define STREAM_VIDEO_SFU_H

#include "stream_video.h"
#include "stream_video_coordinator.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_peer_types.h"
#include "esp_capture_sink.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle for SFU client
 */
typedef struct stream_sfu_client* stream_sfu_client_handle_t;

/**
 * @brief SFU connection state
 */
typedef enum {
    STREAM_SFU_STATE_DISCONNECTED = 0,
    STREAM_SFU_STATE_CONNECTING,
    STREAM_SFU_STATE_CONNECTED,
    STREAM_SFU_STATE_ERROR,
} stream_sfu_state_t;

/**
 * @brief SFU event types
 */
typedef enum {
    STREAM_SFU_EVENT_CONNECTED,
    STREAM_SFU_EVENT_DISCONNECTED,
    STREAM_SFU_EVENT_MESSAGE_RECEIVED,
    STREAM_SFU_EVENT_ERROR,
} stream_sfu_event_type_t;

/**
 * @brief SFU event data
 */
typedef struct {
    stream_sfu_event_type_t type;
    const void *data;
    size_t data_len;
} stream_sfu_event_t;

/**
 * @brief Callback function for SFU events
 */
typedef void (*stream_sfu_event_cb_t)(
    stream_sfu_client_handle_t client,
    const stream_sfu_event_t *event,
    void *user_data
);

/**
 * @brief SFU client configuration
 */
typedef struct {
    const char *ws_endpoint;            // SFU WebSocket URL (from joinCall response)
    const char *token;                  // SFU token (from joinCall response)
    stream_sfu_event_cb_t event_cb;     // Event callback
    void *user_data;                    // User context
    uint32_t reconnect_interval_ms;     // Delay between reconnect attempts (0 = default 5000 ms)
    uint32_t reconnect_max_attempts;    // Max reconnect attempts (0 = infinite)
} stream_sfu_config_t;

/**
 * @brief Create SFU client
 * 
 * @param config Configuration
 * @param client_out Output client handle
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_sfu_client_create(
    const stream_sfu_config_t *config,
    stream_sfu_client_handle_t *client_out
);

/**
 * @brief Destroy SFU client
 * 
 * @param client Client handle
 */
void stream_sfu_client_destroy(stream_sfu_client_handle_t client);

/**
 * @brief Connect to SFU server
 * 
 * @param client Client handle
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_sfu_client_connect(stream_sfu_client_handle_t client);

/**
 * @brief Disconnect from SFU server
 * 
 * @param client Client handle
 */
void stream_sfu_client_disconnect(stream_sfu_client_handle_t client);

/**
 * @brief Send message to SFU server
 * 
 * @param client Client handle
 * @param message Message data
 * @param message_len Message length
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_sfu_client_send(
    stream_sfu_client_handle_t client,
    const uint8_t *message,
    size_t message_len
);

/**
 * @brief Send SFU join request (protobuf)
 *
 * @param client SFU client handle
 * @param token SFU token
 * @param session_id Call session ID
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_sfu_client_send_join_request(
    stream_sfu_client_handle_t client,
    const char *token,
    const char *session_id
);

/**
 * @brief Store join request info (sent on connect)
 *
 * @param client SFU client handle
 * @param token SFU token
 * @param session_id Call session ID
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_sfu_client_set_join_info(
    stream_sfu_client_handle_t client,
    const char *token,
    const char *session_id
);

/**
 * @brief Set SFU HTTP base URL for signaling (Twirp)
 */
stream_video_error_t stream_sfu_client_set_http_url(
    stream_sfu_client_handle_t client,
    const char *http_url
);

/**
 * @brief Set ICE servers for peer connection
 */
stream_video_error_t stream_sfu_client_set_ice_servers(
    stream_sfu_client_handle_t client,
    const esp_peer_ice_server_cfg_t *servers,
    size_t server_count
);

/**
 * @brief Start publishing audio/video from an esp_capture sink
 */
stream_video_error_t stream_sfu_client_start_publishing(
    stream_sfu_client_handle_t client,
    esp_capture_sink_handle_t sink,
    bool publish_audio,
    bool publish_video
);

/**
 * @brief Stop publishing and release publisher peer
 */
stream_video_error_t stream_sfu_client_stop_publishing(stream_sfu_client_handle_t client);

/**
 * @brief Get SFU connection state
 * 
 * @param client Client handle
 * @return stream_sfu_state_t Current state
 */
stream_sfu_state_t stream_sfu_client_get_state(stream_sfu_client_handle_t client);

#ifdef __cplusplus
}
#endif

#endif // STREAM_VIDEO_SFU_H

