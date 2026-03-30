/**
 * @file stream_video.h
 * @brief Stream Video SDK for ESP32 - Main public API
 *
 * This SDK provides real-time video and audio publishing for ESP32-S3 / ESP32-P4
 * devices. It connects to Stream's SFU over WebRTC, captures camera and microphone
 * input using the built-in capture pipeline, and publishes H.264 video and Opus audio
 * to a Stream Video call.
 */

#ifndef STREAM_VIDEO_H
#define STREAM_VIDEO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declaration for join params; include stream_video_auth.h for full type */
struct stream_video_auth_data;

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

/**
 * @brief Parameters for joining a call
 *
 * The app must obtain auth data (userId, apiKey, token) from its own backend
 * or Stream's token service and pass it in \p auth_data. The SDK does not fetch
 * tokens.
 *
 * The SDK automatically prepares the camera/microphone capture pipeline and
 * starts publishing when the SFU connection is established. Use \p mute_audio
 * and \p mute_video to control which media streams are published.
 */
typedef struct {
    const struct stream_video_auth_data *auth_data;
    const char *call_type;     /**< Call type (e.g., "default") */
    const char *call_id;       /**< Call ID (NULL = create new) */
    bool create;               /**< Create call if it doesn't exist */
    const char *location;      /**< Optional location hint (NULL = auto) */
    stream_video_join_result_cb_t result_cb; /**< Result callback (optional) */
    void *user_data;           /**< User context for callback */
    bool mute_audio;           /**< If true, do not publish audio */
    bool mute_video;           /**< If true, do not publish video */
} stream_video_join_call_params_t;

/**
 * @brief Initialize Stream Video SDK
 *
 * Must be called before any other Stream Video SDK function.
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
 * @brief Join a call
 *
 * Connects to the coordinator, joins the call, connects to the SFU, and
 * begins publishing captured media automatically.
 *
 * @param params Join parameters (must include auth_data)
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

#ifdef __cplusplus
}
#endif

#endif // STREAM_VIDEO_H
