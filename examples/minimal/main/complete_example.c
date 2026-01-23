/**
 * @file complete_example.c
 * @brief Example using SDK join/leave APIs
 *
 * This example keeps the app layer minimal. The SDK handles auth,
 * coordinator connect, joinCall, and SFU connect.
 */

#include "stream_video.h"
#include "stream_video_token.h"
#include "esp_log.h"

static const char *TAG = "complete_example";

// Configuration
#define STREAM_ENVIRONMENT "pronto"
#define STREAM_USER_ID "esp32_s3"
#define STREAM_CALL_TYPE "default"
#define STREAM_CALL_ID "79cYh3J5JgGk"

/**
 * @brief Start the SDK-managed join flow
 *
 * Call this from app_main after WiFi is connected.
 */
stream_video_error_t example_complete_flow_start(void)
{
    stream_video_join_call_params_t params = {
        .environment = STREAM_ENVIRONMENT,
        .user_id = STREAM_USER_ID,
        .exp = STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS,
        .call_type = STREAM_CALL_TYPE,
        .call_id = STREAM_CALL_ID,
        .create = true,
        .location = NULL,
        .result_cb = NULL,
        .user_data = NULL,
    };

    stream_video_client_handle_t client = NULL;
    stream_video_error_t err = stream_video_join_call(&params, &client);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to start join flow: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "Join flow started");
    return STREAM_VIDEO_ERR_OK;
}
/**
 * @file complete_example.c
 * @brief Complete example: Auth + Coordinator + SFU + Join Call
 * 
 * This example demonstrates the complete flow matching Android demo app:
 * 1. Request auth data (with user and environment selection)
 * 2. Connect to coordinator WebSocket
 * 3. Join call (with call_type and call_id selection)
 * 4. Connect to SFU WebSocket
 * 5. WebRTC negotiation
 * 
 * Configuration (matches Android demo app):
 * - User: Selected via auth request (user_id parameter)
 * - Environment: Selected via auth request (environment parameter)
 * - Call ID: Specified when joining call
 * - Call Type: Specified when joining call (e.g., "default")
 */

#include "stream_video_auth.h"
#include "stream_video_signaling.h"
#include "stream_video_coordinator.h"
#include "stream_video_sfu.h"
#include "stream_video_token.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "complete_example";

// Global state
static stream_video_auth_data_t g_auth_data = {0};
static bool g_auth_data_received = false;
static stream_signaling_client_handle_t g_coordinator_client = NULL;
static stream_sfu_client_handle_t g_sfu_client = NULL;

// Forward declarations
static void on_sfu_event(stream_sfu_client_handle_t client,
                        const stream_sfu_event_t *event,
                        void *user_data);

// ============================================================================
// Configuration - Set these values (matches Android demo app pattern)
// ============================================================================

// User and Environment Selection (for auth request)
#define STREAM_ENVIRONMENT "pronto"  // "production", "staging", or "development"
#define STREAM_USER_ID "esp32_s3"         // User ID to authenticate as (can be NULL for auto-generated)

// Call Selection (for joining call)
#define STREAM_CALL_TYPE "default"       // Call type (e.g., "default", "livestream")
#define STREAM_CALL_ID "79cYh3J5JgGk"        // Call ID to join (can be NULL to create new call)

// ============================================================================
// Auth Callback
// ============================================================================

/**
 * @brief Callback when auth data is received
 */
static stream_video_error_t on_auth_data_received(
    const stream_video_auth_data_t *auth_data,
    void *user_data)
{
    if (!auth_data) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "✓ Auth data received");
    ESP_LOGI(TAG, "  User ID: %s", auth_data->user_id);
    ESP_LOGI(TAG, "  API Key: %s", auth_data->api_key);
    ESP_LOGI(TAG, "  Token: %.*s...", 20, auth_data->token);

    // Store auth data
    memcpy(&g_auth_data, auth_data, sizeof(stream_video_auth_data_t));
    g_auth_data_received = true;

    // Now that we have auth data, connect to coordinator
    ESP_LOGI(TAG, "Connecting to coordinator WebSocket...");

    stream_signaling_config_t coordinator_config = {
        .api_key = g_auth_data.api_key,
        .user_id = g_auth_data.user_id,
        .token = g_auth_data.token,
        .coordinator_url = NULL,  // Use default: wss://video.stream-io-api.com/video/connect
        .event_cb = on_coordinator_event,
        .user_data = NULL,
        .reconnect_interval_ms = 5000,
        .reconnect_max_attempts = 10,
    };

    stream_video_error_t err = stream_signaling_client_create(&coordinator_config, &g_coordinator_client);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to create coordinator client: %d", err);
        return err;
    }

    err = stream_signaling_client_connect(g_coordinator_client);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to connect to coordinator: %d", err);
        return err;
    }

    return STREAM_VIDEO_ERR_OK;
}

// ============================================================================
// Coordinator Event Callback
// ============================================================================

/**
 * @brief Coordinator event callback
 */
static void on_coordinator_event(stream_signaling_client_handle_t client,
                                 const stream_signaling_event_t *event,
                                 void *user_data)
{
    switch (event->type) {
        case STREAM_SIGNALING_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✓ Connected to coordinator WebSocket");
            
            // Send connect message to authenticate
            {
                uint8_t *message = NULL;
                size_t message_len = 0;
                
                stream_video_error_t err = stream_signaling_create_connect_message(
                    g_auth_data.user_id,
                    g_auth_data.token,
                    NULL,  // session_id (optional)
                    &message,
                    &message_len
                );
                
                if (err == STREAM_VIDEO_ERR_OK && message) {
                    ESP_LOGI(TAG, "Sending connect message to coordinator...");
                    stream_signaling_client_send(client, message, message_len);
                    free(message);
                } else {
                    ESP_LOGE(TAG, "Failed to create connect message: %d", err);
                }
            }
            break;
            
        case STREAM_SIGNALING_EVENT_MESSAGE_RECEIVED:
            ESP_LOGI(TAG, "Coordinator message received");
            // Message handled by stream_signaling_handle_message
            break;
            
        case STREAM_SIGNALING_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "✗ Disconnected from coordinator");
            break;
            
        default:
            break;
    }
}

// ============================================================================
// Join Call Response Callback
// ============================================================================

/**
 * @brief Join call response callback
 */
static stream_video_error_t on_join_call_response(
    const stream_video_join_call_response_t *response,
    void *user_data)
{
    if (!response) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    if (response->success) {
        ESP_LOGI(TAG, "✓ Join call successful");
        ESP_LOGI(TAG, "  Call ID: %s", response->call_id);
        ESP_LOGI(TAG, "  SFU WebSocket: %s", response->credentials.ws_endpoint);
        ESP_LOGI(TAG, "  SFU HTTP: %s", response->credentials.http_url);
        
        // Connect to SFU WebSocket
        stream_sfu_config_t sfu_config = {
            .ws_endpoint = response->credentials.ws_endpoint,
            .token = response->credentials.token,
            .event_cb = on_sfu_event,
            .user_data = NULL,
        };
        
        stream_video_error_t err = stream_sfu_client_create(&sfu_config, &g_sfu_client);
        if (err != STREAM_VIDEO_ERR_OK) {
            ESP_LOGE(TAG, "Failed to create SFU client: %d", err);
            return err;
        }
        
        err = stream_sfu_client_connect(g_sfu_client);
        if (err != STREAM_VIDEO_ERR_OK) {
            ESP_LOGE(TAG, "Failed to connect to SFU: %d", err);
            return err;
        }
        
        ESP_LOGI(TAG, "✓ Connecting to SFU WebSocket...");
    } else {
        ESP_LOGE(TAG, "Join call failed: %s", response->error_message);
    }
    
    return STREAM_VIDEO_ERR_OK;
}

// ============================================================================
// SFU Event Callback
// ============================================================================

/**
 * @brief SFU event callback
 */
static void on_sfu_event(stream_sfu_client_handle_t client,
                        const stream_sfu_event_t *event,
                        void *user_data)
{
    switch (event->type) {
        case STREAM_SFU_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✓ Connected to SFU WebSocket");
            ESP_LOGI(TAG, "Ready for WebRTC negotiation");
            // TODO: Start WebRTC negotiation
            break;
            
        case STREAM_SFU_EVENT_MESSAGE_RECEIVED:
            ESP_LOGI(TAG, "SFU message received (SDP/ICE)");
            // TODO: Handle SDP offers/answers and ICE candidates
            break;
            
        case STREAM_SFU_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "✗ Disconnected from SFU");
            break;
            
        default:
            break;
    }
}

// ============================================================================
// Main Flow
// ============================================================================

/**
 * @brief Start complete flow
 * 
 * This function starts the complete flow:
 * 1. Request auth data (with user and environment)
 * 2. Connect to coordinator
 * 3. Join call (with call_type and call_id)
 * 4. Connect to SFU
 * 
 * Configuration is done via #defines at the top of this file:
 * - STREAM_ENVIRONMENT: Environment name
 * - STREAM_USER_ID: User ID (or NULL for auto-generated)
 * - STREAM_CALL_TYPE: Call type
 * - STREAM_CALL_ID: Call ID (or NULL to create new)
 */
stream_video_error_t example_complete_flow_start(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Stream Video ESP32 - Complete Flow");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  Environment: %s", STREAM_ENVIRONMENT);
    ESP_LOGI(TAG, "  User ID: %s", STREAM_USER_ID ? STREAM_USER_ID : "NULL (auto-generated)");
    ESP_LOGI(TAG, "  Call Type: %s", STREAM_CALL_TYPE);
    ESP_LOGI(TAG, "  Call ID: %s", STREAM_CALL_ID ? STREAM_CALL_ID : "NULL (create new)");
    ESP_LOGI(TAG, "========================================");

    // Step 1: Request auth data (with user and environment selection)
    ESP_LOGI(TAG, "Step 1: Requesting auth data...");
    
    stream_video_auth_request_t auth_req = {
        .environment = STREAM_ENVIRONMENT,                    // Selected environment
        .user_id = STREAM_USER_ID,                            // Selected user ID (or NULL)
        .exp = STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS,    // 7 days
    };

    stream_video_error_t err = stream_video_request_auth_data(
        &auth_req,
        on_auth_data_received,
        NULL
    );

    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to request auth data: %d", err);
        return err;
    }

    // Wait for auth data (in real app, this would be async)
    // For this example, we'll wait a bit
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    if (!g_auth_data_received) {
        ESP_LOGE(TAG, "Auth data not received yet");
        return STREAM_VIDEO_ERR_FAIL;
    }

    // Step 2: Coordinator connection happens in on_auth_data_received callback
    // Step 3: After coordinator is connected, join call
    
    // Wait for coordinator to connect
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Step 3: Join call (with call_type and call_id selection)
    ESP_LOGI(TAG, "Step 3: Joining call...");
    ESP_LOGI(TAG, "  Call Type: %s", STREAM_CALL_TYPE);
    ESP_LOGI(TAG, "  Call ID: %s", STREAM_CALL_ID ? STREAM_CALL_ID : "NULL (will create new)");

    stream_video_join_call_request_t join_req = {
        .api_key = g_auth_data.api_key,
        .user_id = g_auth_data.user_id,
        .token = g_auth_data.token,
        .call_type = STREAM_CALL_TYPE,    // Selected call type
        .call_id = STREAM_CALL_ID,        // Selected call ID (or NULL)
        .create = true,                    // Create if doesn't exist
    };

    err = stream_video_coordinator_join_call(&join_req, on_join_call_response, NULL);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to join call: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "✓ Join call request sent");
    ESP_LOGI(TAG, "Step 4: SFU connection will happen in callback");

    return STREAM_VIDEO_ERR_OK;
}

/**
 * @brief Example usage in app_main
 * 
 * In your app_main, call this function to start the complete flow:
 * 
 * ```c
 * void app_main(void) {
 *     // Initialize SDK, WiFi, etc.
 *     stream_video_init();
 *     wifi_init();
 *     
 *     // Wait for WiFi
 *     // ...
 *     
 *     // Start complete flow
 *     example_complete_flow_start();
 * }
 * ```
 */
