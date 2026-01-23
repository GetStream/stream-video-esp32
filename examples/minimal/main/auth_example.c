/**
 * @file auth_example.c
 * @brief Example: Request auth data and create user
 * 
 * This example shows how to request auth data from backend and create a user object,
 * matching the Android demo app pattern.
 */

#include "stream_video_auth.h"
#include "stream_video_signaling.h"
#include "stream_video_token.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "auth_example";

// Auth data received from backend
static stream_video_auth_data_t g_auth_data = {0};
static bool g_auth_data_received = false;

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

    return STREAM_VIDEO_ERR_OK;
}

/**
 * @brief Request auth data and create user
 * 
 * This function demonstrates the complete flow:
 * 1. Request auth data from backend (GET api/auth/create-token)
 * 2. Create user object from auth data
 * 3. Use auth data for signaling connection
 */
stream_video_error_t example_request_auth_and_create_user(
    const char *environment,
    const char *user_id)
{
    ESP_LOGI(TAG, "Requesting auth data...");

    // Prepare auth request (matches Android demo app)
    stream_video_auth_request_t auth_req = {
        .environment = environment,                    // e.g., "production", "staging"
        .user_id = user_id,                           // Optional: can be NULL
        .exp = STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS,  // 7 days
    };

    // Request auth data from backend
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
    // For this example, we assume it's received synchronously
    if (!g_auth_data_received) {
        ESP_LOGE(TAG, "Auth data not received");
        return STREAM_VIDEO_ERR_FAIL;
    }

    // Create user object from auth data (matches Android SDK pattern)
    // User(id = authData.userId, role = "admin")
    stream_video_user_t user = {0};
    err = stream_video_create_user_from_auth(
        &g_auth_data,
        STREAM_VIDEO_USER_ROLE_ADMIN,  // Default role: admin
        &user
    );

    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to create user: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "✓ User created: id=%s, role=%d", user.id, user.role);

    // Now you can use auth_data.api_key and auth_data.token for signaling
    stream_signaling_config_t signaling_config = {
        .api_key = g_auth_data.api_key,
        .user_id = g_auth_data.user_id,
        .token = g_auth_data.token,
        .env = STREAM_SIGNALING_ENV_PRODUCTION,
        .coordinator_url = NULL,
        .event_cb = NULL,  // Set your event callback
        .user_data = NULL,
        .reconnect_interval_ms = 5000,
        .reconnect_max_attempts = 10,
    };

    stream_signaling_client_handle_t signaling_client;
    err = stream_signaling_client_create(&signaling_config, &signaling_client);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to create signaling client: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "✓ Signaling client created with auth data");

    // Connect to signaling server
    err = stream_signaling_client_connect(signaling_client);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to connect: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "✓ Connected to signaling server");

    return STREAM_VIDEO_ERR_OK;
}

