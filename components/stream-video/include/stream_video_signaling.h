/**
 * @file stream_video_signaling.h
 * @brief Stream Video Signaling API
 * 
 * This module handles WebSocket-based signaling for Stream Video SDK.
 * It manages connection, authentication, and message exchange with Stream's signaling server.
 */

#ifndef STREAM_VIDEO_SIGNALING_H
#define STREAM_VIDEO_SIGNALING_H

#include "stream_video.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle for signaling client
 */
typedef struct stream_signaling_client* stream_signaling_client_handle_t;

/**
 * @brief Signaling connection state
 */
typedef enum {
    STREAM_SIGNALING_STATE_DISCONNECTED = 0,
    STREAM_SIGNALING_STATE_CONNECTING,
    STREAM_SIGNALING_STATE_CONNECTED,
    STREAM_SIGNALING_STATE_RECONNECTING,
    STREAM_SIGNALING_STATE_ERROR,
} stream_signaling_state_t;

/**
 * @brief Signaling event types
 */
typedef enum {
    STREAM_SIGNALING_EVENT_CONNECTED,
    STREAM_SIGNALING_EVENT_DISCONNECTED,
    STREAM_SIGNALING_EVENT_MESSAGE_RECEIVED,
    STREAM_SIGNALING_EVENT_ERROR,
} stream_signaling_event_type_t;

/**
 * @brief Signaling event data
 */
typedef struct {
    stream_signaling_event_type_t type;
    const void *data;
    size_t data_len;
} stream_signaling_event_t;

/**
 * @brief Callback function for signaling events
 * 
 * @param client Signaling client handle
 * @param event Event data
 * @param user_data User-provided context
 */
typedef void (*stream_signaling_event_cb_t)(
    stream_signaling_client_handle_t client,
    const stream_signaling_event_t *event,
    void *user_data
);

/**
 * @brief Environment type for Stream service
 */
typedef enum {
    STREAM_SIGNALING_ENV_PRODUCTION = 0,
    STREAM_SIGNALING_ENV_STAGING,
    STREAM_SIGNALING_ENV_DEVELOPMENT,
} stream_signaling_environment_t;

/**
 * @brief Signaling client configuration
 * 
 * Note: Matches Stream Android SDK pattern - uses coordinator WebSocket for signaling.
 * Coordinator URL defaults to wss://video.stream-io-api.com/video/connect
 */
typedef struct {
    const char *api_key;                // Stream API key
    const char *user_id;                // User ID
    const char *token;                  // Authentication token (JWT with expiry)
    stream_signaling_environment_t env; // Environment (production/staging/development)
    const char *coordinator_url;         // Optional: Custom coordinator URL (NULL = use default)
    stream_signaling_event_cb_t event_cb; // Event callback
    void *user_data;                    // User context for callbacks
    uint32_t reconnect_interval_ms;     // Reconnection interval (default: 5000ms)
    uint32_t reconnect_max_attempts;     // Max reconnection attempts (0 = infinite)
} stream_signaling_config_t;

/**
 * @brief Create a signaling client
 * 
 * @param config Configuration
 * @param client_out Output client handle
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_signaling_client_create(
    const stream_signaling_config_t *config,
    stream_signaling_client_handle_t *client_out
);

/**
 * @brief Destroy a signaling client
 * 
 * @param client Client handle
 */
void stream_signaling_client_destroy(stream_signaling_client_handle_t client);

/**
 * @brief Connect to signaling server
 * 
 * @param client Client handle
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_signaling_client_connect(stream_signaling_client_handle_t client);

/**
 * @brief Disconnect from signaling server
 * 
 * @param client Client handle
 */
void stream_signaling_client_disconnect(stream_signaling_client_handle_t client);

/**
 * @brief Send a message to signaling server
 * 
 * @param client Client handle
 * @param message Message data
 * @param message_len Message length
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_signaling_client_send(
    stream_signaling_client_handle_t client,
    const uint8_t *message,
    size_t message_len
);

/**
 * @brief Get current connection state
 * 
 * @param client Client handle
 * @return stream_signaling_state_t Current state
 */
stream_signaling_state_t stream_signaling_client_get_state(stream_signaling_client_handle_t client);

/**
 * @brief Get coordinator connection ID
 *
 * @param client Signaling client handle
 * @return const char* Connection ID or NULL
 */
const char *stream_signaling_client_get_connection_id(stream_signaling_client_handle_t client);

/**
 * @brief Set coordinator connection ID (internal)
 *
 * @param client Signaling client handle
 * @param connection_id Connection ID
 */
void stream_signaling_client_set_connection_id(
    stream_signaling_client_handle_t client,
    const char *connection_id);

/**
 * @brief Check if client is connected
 * 
 * @param client Client handle
 * @return true if connected, false otherwise
 */
bool stream_signaling_client_is_connected(stream_signaling_client_handle_t client);

/**
 * @brief Process incoming signaling message
 * 
 * @param client Signaling client
 * @param message Message data
 * @param message_len Message length
 */
void stream_signaling_handle_message(
    stream_signaling_client_handle_t client,
    const uint8_t *message,
    size_t message_len);

/**
 * @brief Create a connect message
 * 
 * @param user_id User ID
 * @param token Authentication token
 * @param session_id Session ID (optional, can be NULL)
 * @param message_out Output message buffer (caller must free)
 * @param message_len_out Output message length
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_signaling_create_connect_message(
    const char *user_id,
    const char *token,
    const char *session_id,
    uint8_t **message_out,
    size_t *message_len_out);

/**
 * @brief Create a join room message
 * 
 * @param room_id Room ID
 * @param user_id User ID
 * @param message_out Output message buffer (caller must free)
 * @param message_len_out Output message length
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_signaling_create_join_room_message(
    const char *room_id,
    const char *user_id,
    uint8_t **message_out,
    size_t *message_len_out);

/**
 * @brief Create a publish track message
 * 
 * @param track_id Track ID
 * @param kind Track kind ("audio" or "video")
 * @param track_type Track type (optional, can be NULL)
 * @param message_out Output message buffer (caller must free)
 * @param message_len_out Output message length
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_signaling_create_publish_track_message(
    const char *track_id,
    const char *kind,
    const char *track_type,
    uint8_t **message_out,
    size_t *message_len_out);

#ifdef __cplusplus
}
#endif

#endif // STREAM_VIDEO_SIGNALING_H

