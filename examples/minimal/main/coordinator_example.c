/**
 * @file coordinator_example.c
 * @brief Example: Complete flow with coordinator and SFU
 * 
 * This example demonstrates the complete flow matching Android SDK:
 * 1. Connect to coordinator WebSocket
 * 2. Join call via coordinator REST API
 * 3. Connect to SFU WebSocket
 * 4. WebRTC negotiation
 */

#include "stream_video_auth.h"
#include "stream_video_signaling.h"
#include "stream_video_coordinator.h"
#include "stream_video_sfu.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "coordinator_example";

static stream_signaling_client_handle_t g_coordinator_client = NULL;
static stream_sfu_client_handle_t g_sfu_client = NULL;
static stream_video_sfu_credentials_t g_sfu_credentials = {0};

// Forward declaration
static void on_sfu_event(stream_sfu_client_handle_t client,
                        const stream_sfu_event_t *event,
                        void *user_data);

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
            
            // Send connect message
            {
                uint8_t *message = NULL;
                size_t message_len = 0;
                // Get user_id and token from config (stored in client)
                // For now, this is a placeholder - actual implementation needs to store these
                ESP_LOGI(TAG, "Ready to send connect message");
            }
            break;
            
        case STREAM_SIGNALING_EVENT_MESSAGE_RECEIVED:
            ESP_LOGI(TAG, "Coordinator message received");
            break;
            
        default:
            break;
    }
}

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
        
        // Store SFU credentials
        memcpy(&g_sfu_credentials, &response->credentials, sizeof(g_sfu_credentials));
        
        // Connect to SFU WebSocket
        stream_sfu_config_t sfu_config = {
            .ws_endpoint = g_sfu_credentials.ws_endpoint,
            .token = g_sfu_credentials.token,
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
            
        default:
            break;
    }
}

/**
 * @brief Complete flow example
 */
stream_video_error_t example_complete_flow(
    const stream_video_auth_data_t *auth_data,
    const char *call_type,
    const char *call_id)
{
    ESP_LOGI(TAG, "Starting complete flow...");

    // Step 1: Connect to coordinator WebSocket
    stream_signaling_config_t coordinator_config = {
        .api_key = auth_data->api_key,
        .user_id = auth_data->user_id,
        .token = auth_data->token,
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

    ESP_LOGI(TAG, "✓ Coordinator client created, connecting...");

    // Step 2: Join call via coordinator REST API
    // (This should be called after coordinator is connected)
    stream_video_join_call_request_t join_req = {
        .api_key = auth_data->api_key,
        .user_id = auth_data->user_id,
        .token = auth_data->token,
        .call_type = call_type,
        .call_id = call_id,
        .create = true,
    };

    err = stream_video_coordinator_join_call(&join_req, on_join_call_response, NULL);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to join call: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "✓ Join call request sent");

    // Step 3: SFU connection happens in on_join_call_response callback
    // Step 4: WebRTC negotiation happens after SFU is connected

    return STREAM_VIDEO_ERR_OK;
}

