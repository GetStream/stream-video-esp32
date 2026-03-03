/**
 * @file stream_video.h
 * @brief Stream Video SDK for ESP32 - Main public API
 *
 * This SDK provides real-time video, audio, and data capabilities for ESP32-S3 devices.
 *
 * **Capture sink and publishing**
 * The SDK sends (publishes) your audio/video to the call by reading encoded frames from
 * a "capture sink". In the ESP capture framework, the pipeline is: sources (camera, mic)
 * -> capture/encode -> sink. The sink is the output handle from which the SDK acquires
 * H264/Opus frames and sends them over WebRTC. So "sink" is the source of media for
 * publishing. You can either pass a sink in join params (after preparing capture yourself)
 * or register a capture provider so the SDK prepares capture when needed.
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

/**
 * @brief Capture provider: prepare capture and return sink (SDK calls when needed)
 * @param user_data User context passed to stream_video_set_capture_provider
 * @param sink_out  On success, set to the capture sink handle
 * @return stream_video_error_t STREAM_VIDEO_ERR_OK on success
 */
typedef stream_video_error_t (*stream_video_capture_prepare_cb_t)(
    void *user_data,
    esp_capture_sink_handle_t *sink_out
);

/**
 * @brief Capture provider: stop capture and release resources (SDK calls on leave)
 * @param user_data User context passed to stream_video_set_capture_provider
 */
typedef void (*stream_video_capture_stop_cb_t)(void *user_data);

/**
 * @brief Parameters for joining a call (SDK-managed flow)
 *
 * Publishing starts automatically when the SFU is connected. The SDK needs a
 * capture sink to read encoded frames from. You can either:
 * - Pass \p sink in the params (you prepare capture before join), or
 * - Register a capture provider with stream_video_set_capture_provider(); then
 *   leave \p sink NULL and the SDK will call the provider when it needs a sink.
 * If \p sink is NULL and no provider is set, no media is published (receive-only).
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
    esp_capture_sink_handle_t sink;  // Optional: capture sink (NULL = use provider or no publish)
    bool mute_audio;           // If true, do not publish audio
    bool mute_video;           // If true, do not publish video
} stream_video_join_call_params_t;

/**
 * @brief Initialize Stream Video SDK
 *
 * This must be called before using any other Stream Video SDK functions.
 *
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_video_init(void);

/**
 * @brief Register optional capture provider so the SDK prepares capture when needed
 *
 * If set, the SDK will call \p prepare_cb when it needs a sink for publishing
 * (during join, when SFU is connected), and \p stop_cb when leaving the call.
 * The app then only needs to call join/leave with mute flags; no need to prepare
 * capture or pass a sink. Pass NULL for both to clear the provider.
 *
 * @param prepare_cb Called when SDK needs a sink; set *sink_out and return OK
 * @param stop_cb    Called when leaving the call to release capture
 * @param user_data  Passed to both callbacks
 */
void stream_video_set_capture_provider(
    stream_video_capture_prepare_cb_t prepare_cb,
    stream_video_capture_stop_cb_t stop_cb,
    void *user_data);

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

#ifdef __cplusplus
}
#endif

#endif // STREAM_VIDEO_H

