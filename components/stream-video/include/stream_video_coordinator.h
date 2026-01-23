/**
 * @file stream_video_coordinator.h
 * @brief Coordinator WebSocket and REST API
 * 
 * Handles connection to Stream's coordinator service for signaling and SFU discovery.
 * Matches Android SDK pattern with coordinator WebSocket and joinCall REST API.
 */

#ifndef STREAM_VIDEO_COORDINATOR_H
#define STREAM_VIDEO_COORDINATOR_H

#include "stream_video.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default coordinator WebSocket URL (matches Android SDK)
 */
#define STREAM_VIDEO_COORDINATOR_WS_URL "wss://video.stream-io-api.com/video/connect"

/**
 * @brief Default coordinator REST API base URL
 */
#define STREAM_VIDEO_COORDINATOR_REST_URL "https://video.stream-io-api.com"

/**
 * @brief SFU credentials from joinCall response
 * 
 * Matches Android SDK JoinCallResponse.credentials structure.
 */
typedef struct {
    char ws_endpoint[256];      // SFU WebSocket URL (credentials.server.wsEndpoint)
    char http_url[256];         // SFU HTTP API URL (credentials.server.url)
    char token[2048];           // SFU authentication token (credentials.token)
    char ice_servers[1024];     // STUN/TURN servers JSON (credentials.iceServers)
} stream_video_sfu_credentials_t;

/**
 * @brief Join call request parameters
 */
typedef struct {
    const char *api_key;        // Stream API key
    const char *user_id;        // User ID
    const char *token;          // Authentication token
    const char *call_type;      // Call type (e.g., "default")
    const char *call_id;       // Call ID (optional, can be NULL)
    bool create;                // Create call if it doesn't exist
    const char *location;       // Location hint (optional, NULL = auto)
    const char *connection_id;  // Coordinator connection ID (optional)
} stream_video_join_call_request_t;

/**
 * @brief Join call response
 * 
 * Matches Android SDK JoinCallResponse.
 */
typedef struct {
    bool success;                           // Join successful
    char call_id[128];                      // Call ID
    stream_video_sfu_credentials_t credentials; // SFU credentials
    char error_message[256];                // Error message if failed
    char session_id[128];                   // Current session ID (if available)
} stream_video_join_call_response_t;

/**
 * @brief Callback for join call response
 * 
 * @param response Join call response
 * @param user_data User-provided context
 * @return stream_video_error_t Error code
 */
typedef stream_video_error_t (*stream_video_join_call_callback_t)(
    const stream_video_join_call_response_t *response,
    void *user_data
);

/**
 * @brief Join a call via coordinator REST API
 * 
 * Makes POST request to: /video/calls/{call_type}/{call_id}/join
 * Matches Android SDK call.join() flow.
 * 
 * @param request Join call request parameters
 * @param callback Callback when response is received
 * @param user_data User context for callback
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_video_coordinator_join_call(
    const stream_video_join_call_request_t *request,
    stream_video_join_call_callback_t callback,
    void *user_data
);

#ifdef __cplusplus
}
#endif

#endif // STREAM_VIDEO_COORDINATOR_H

