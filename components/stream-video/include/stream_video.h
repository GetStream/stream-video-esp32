/**
 * @file stream_video.h
 * @brief Stream Video SDK for ESP32 - Main public API
 * 
 * This SDK provides real-time video, audio, and data capabilities for ESP32-S3 devices.
 */

#ifndef STREAM_VIDEO_H
#define STREAM_VIDEO_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_capture_sink.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error codes returned by Stream Video SDK functions
 */
typedef enum {
    STREAM_VIDEO_ERR_OK = 0,
    STREAM_VIDEO_ERR_INVALID_ARG,
    STREAM_VIDEO_ERR_NO_MEM,
    STREAM_VIDEO_ERR_INVALID_STATE,
    STREAM_VIDEO_ERR_NETWORK,
    STREAM_VIDEO_ERR_TIMEOUT,
    STREAM_VIDEO_ERR_FAIL,
} stream_video_error_t;

/**
 * @brief Opaque handle for Stream Video client
 */
typedef struct stream_video_client* stream_video_client_handle_t;

/**
 * @brief Opaque handle for Stream Video room
 */
typedef struct stream_video_room* stream_video_room_handle_t;

/**
 * @brief Join call result callback
 */
typedef struct {
    bool success;
    char error_message[256];
} stream_video_join_result_t;

typedef void (*stream_video_join_result_cb_t)(
    const stream_video_join_result_t *result,
    void *user_data
);

typedef void (*stream_video_sfu_connected_cb_t)(
    stream_video_client_handle_t client,
    void *user_data
);

/**
 * @brief Parameters for joining a call (SDK-managed flow)
 */
typedef struct {
    const char *environment;   // Environment name (e.g., "production", "staging")
    const char *user_id;       // Optional: User ID (NULL = auto-generate)
    uint32_t exp;              // Token expiry in seconds
    const char *call_type;     // Call type (e.g., "default")
    const char *call_id;       // Call ID (NULL = create new)
    bool create;               // Create call if it doesn't exist
    const char *location;      // Optional: Location hint (NULL = auto)
    stream_video_join_result_cb_t result_cb; // Result callback (optional)
    void *user_data;           // User context for callback
    stream_video_sfu_connected_cb_t sfu_connected_cb; // SFU connected callback (optional)
    void *sfu_user_data;       // User context for SFU callback
} stream_video_join_call_params_t;

/**
 * @brief Parameters for publishing media from esp_capture
 */
typedef struct {
    esp_capture_sink_handle_t sink;
    bool publish_audio;
    bool publish_video;
} stream_video_publish_params_t;

/**
 * @brief Initialize Stream Video SDK
 * 
 * This must be called before using any other Stream Video SDK functions.
 * 
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_video_init(void);

/**
 * @brief Deinitialize Stream Video SDK
 */
void stream_video_deinit(void);

/**
 * @brief Get error string from error code
 * 
 * @param error Error code
 * @return const char* Error string
 */
const char* stream_video_error_to_string(stream_video_error_t error);

/**
 * @brief Join a call (SDK-managed flow)
 *
 * This will handle auth, coordinator connect, joinCall, and SFU connect.
 *
 * @param params Join parameters
 * @param client_out Output client handle
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_video_join_call(
    const stream_video_join_call_params_t *params,
    stream_video_client_handle_t *client_out);

/**
 * @brief Leave a call and clean up resources
 *
 * @param client Client handle from stream_video_join_call
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_video_leave_call(stream_video_client_handle_t client);

/**
 * @brief Start publishing audio/video to the SFU
 */
stream_video_error_t stream_video_start_publishing(
    stream_video_client_handle_t client,
    const stream_video_publish_params_t *params);

/**
 * @brief Wait until the SFU WebSocket is connected
 *
 * @param client Client handle from stream_video_join_call
 * @param timeout_ms Timeout in milliseconds
 * @return stream_video_error_t Error code
 */
/**
 * @brief Stop publishing audio/video
 */
stream_video_error_t stream_video_stop_publishing(stream_video_client_handle_t client);

#ifdef __cplusplus
}
#endif

#endif // STREAM_VIDEO_H

