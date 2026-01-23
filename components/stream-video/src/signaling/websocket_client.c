/**
 * @file websocket_client.c
 * @brief WebSocket client for Stream signaling
 * 
 * Uses ESP-IDF's WebSocket client to connect to Stream's signaling server.
 * Handles connection, reconnection, and message sending/receiving.
 */

#include "stream_video_signaling.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdlib.h>

// Coordinator WebSocket URL (matches Android SDK)
#define STREAM_VIDEO_COORDINATOR_WS_URL "wss://video.stream-io-api.com/video/connect"

static const char *TAG = "stream_signaling_ws";
static const char *STREAM_WS_HEADERS =
    "X-Stream-Client: stream-video-esp32\r\n"
    "stream-auth-type: jwt\r\n";
static const char *STREAM_WS_CLIENT_QUERY = "stream-video-esp32";

// Event bits for WebSocket events
#define WS_CONNECTED_BIT    BIT0
#define WS_DISCONNECTED_BIT BIT1
#define WS_ERROR_BIT        BIT2

/**
 * @brief Signaling client structure
 */
struct stream_signaling_client {
    esp_websocket_client_handle_t ws_client;
    stream_signaling_config_t config;
    char signaling_url[256];           // Constructed signaling URL
    stream_signaling_state_t state;
    EventGroupHandle_t event_group;
    TaskHandle_t reconnect_task;
    TaskHandle_t health_task;
    uint32_t reconnect_attempts;
    bool should_reconnect;
    void *user_data;
    char connection_id[64];
    uint8_t *rx_buffer;
    size_t rx_size;
    size_t rx_offset;
    TickType_t last_event_tick;
};

static void send_health_check(stream_signaling_client_handle_t client)
{
    if (!client) {
        return;
    }

    char message[160];
    if (client->connection_id[0]) {
        snprintf(message, sizeof(message),
                 "{\"type\":\"connection.ok\",\"connection_id\":\"%s\"}",
                 client->connection_id);
    } else {
        snprintf(message, sizeof(message), "{\"type\":\"connection.ok\"}");
    }

    stream_signaling_client_send(client, (const uint8_t *)message, strlen(message));
}

static void health_monitor_task(void *pvParameters)
{
    stream_signaling_client_handle_t client = (stream_signaling_client_handle_t)pvParameters;
    const TickType_t check_interval = pdMS_TO_TICKS(1000);
    const TickType_t no_event_threshold = pdMS_TO_TICKS(30000);

    while (client && client->should_reconnect) {
        vTaskDelay(check_interval);

        if (client->state == STREAM_SIGNALING_STATE_CONNECTED) {
            send_health_check(client);

            TickType_t now = xTaskGetTickCount();
            if ((now - client->last_event_tick) > no_event_threshold) {
                ESP_LOGW(TAG, "No coordinator events for 30s, reconnecting...");
                esp_websocket_client_stop(client->ws_client);
                xEventGroupSetBits(client->event_group, WS_ERROR_BIT);
            }
        }
    }

    vTaskDelete(NULL);
}

/**
 * @brief WebSocket event handler
 */
static void websocket_event_handler(void *handler_args, esp_event_base_t base, 
                                    int32_t event_id, void *event_data)
{
    stream_signaling_client_handle_t client = (stream_signaling_client_handle_t)handler_args;
    esp_websocket_event_id_t ws_event_id = (esp_websocket_event_id_t)event_id;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (ws_event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            client->state = STREAM_SIGNALING_STATE_CONNECTED;
            client->reconnect_attempts = 0;
            client->last_event_tick = xTaskGetTickCount();
            xEventGroupSetBits(client->event_group, WS_CONNECTED_BIT);
            
            // Notify user
            if (client->config.event_cb) {
                stream_signaling_event_t event = {
                    .type = STREAM_SIGNALING_EVENT_CONNECTED,
                    .data = NULL,
                    .data_len = 0
                };
                client->config.event_cb(client, &event, client->config.user_data);
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket disconnected");
            client->state = STREAM_SIGNALING_STATE_DISCONNECTED;
            xEventGroupSetBits(client->event_group, WS_DISCONNECTED_BIT);

            if (client->rx_buffer) {
                free(client->rx_buffer);
                client->rx_buffer = NULL;
                client->rx_size = 0;
                client->rx_offset = 0;
            }
            
            // Notify user
            if (client->config.event_cb) {
                stream_signaling_event_t event = {
                    .type = STREAM_SIGNALING_EVENT_DISCONNECTED,
                    .data = NULL,
                    .data_len = 0
                };
                client->config.event_cb(client, &event, client->config.user_data);
            }
            break;

        case WEBSOCKET_EVENT_DATA:
            ESP_LOGD(TAG, "Received data: opcode=%d, len=%d", data->op_code, data->data_len);
            client->last_event_tick = xTaskGetTickCount();
            
            // Handle the message
            extern void stream_signaling_handle_message(
                stream_signaling_client_handle_t client,
                const uint8_t *message,
                size_t message_len);
            
            if (data->data_ptr && data->data_len > 0) {
                if (data->op_code != 0x1) {
                    ESP_LOGW(TAG, "Skipping non-text WS message (opcode=%d)", data->op_code);
                    break;
                }

                const int payload_len = data->payload_len > 0 ? data->payload_len : data->data_len;
                if (data->payload_offset == 0) {
                    if (client->rx_buffer) {
                        free(client->rx_buffer);
                        client->rx_buffer = NULL;
                    }
                    client->rx_size = 0;
                    client->rx_offset = 0;

                    if (payload_len <= 8192) {
                        client->rx_buffer = (uint8_t *)malloc((size_t)payload_len + 1);
                        if (client->rx_buffer) {
                            client->rx_size = (size_t)payload_len + 1;
                        } else {
                            ESP_LOGE(TAG, "Failed to allocate WS payload buffer");
                        }
                    } else {
                        ESP_LOGW(TAG, "WS payload too large (%d), skipping", payload_len);
                    }
                }

                if (client->rx_buffer && client->rx_offset + data->data_len < client->rx_size) {
                    memcpy(client->rx_buffer + client->rx_offset, data->data_ptr, data->data_len);
                    client->rx_offset += data->data_len;

                    if (data->payload_offset + data->data_len >= data->payload_len) {
                        client->rx_buffer[client->rx_offset] = '\0';

                        size_t log_len = client->rx_offset > 1024 ? 1024 : client->rx_offset;
                        ESP_LOGI(TAG, "WS payload (%u bytes): %.*s",
                                 (unsigned)client->rx_offset,
                                 (int)log_len,
                                 (const char *)client->rx_buffer);
                        if (client->rx_offset > log_len) {
                            ESP_LOGI(TAG, "WS payload truncated in log (%u bytes total)",
                                     (unsigned)client->rx_offset);
                        }

                        stream_signaling_handle_message(client, client->rx_buffer, client->rx_offset);

                        if (client->config.event_cb) {
                            stream_signaling_event_t event = {
                                .type = STREAM_SIGNALING_EVENT_MESSAGE_RECEIVED,
                                .data = client->rx_buffer,
                                .data_len = client->rx_offset
                            };
                            client->config.event_cb(client, &event, client->config.user_data);
                        }

                        free(client->rx_buffer);
                        client->rx_buffer = NULL;
                        client->rx_size = 0;
                        client->rx_offset = 0;
                    }
                } else if (client->rx_buffer) {
                    ESP_LOGW(TAG, "WS payload truncated (buffer too small)");
                }
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error");
            client->state = STREAM_SIGNALING_STATE_ERROR;
            xEventGroupSetBits(client->event_group, WS_ERROR_BIT);

            if (client->rx_buffer) {
                free(client->rx_buffer);
                client->rx_buffer = NULL;
                client->rx_size = 0;
                client->rx_offset = 0;
            }
            
            // Notify user
            if (client->config.event_cb) {
                stream_signaling_event_t event = {
                    .type = STREAM_SIGNALING_EVENT_ERROR,
                    .data = NULL,
                    .data_len = 0
                };
                client->config.event_cb(client, &event, client->config.user_data);
            }
            break;

        default:
            break;
    }
}

/**
 * @brief Reconnection task
 */
static void reconnect_task(void *pvParameters)
{
    stream_signaling_client_handle_t client = (stream_signaling_client_handle_t)pvParameters;
    
    while (client->should_reconnect) {
        // Wait for disconnection
        xEventGroupWaitBits(client->event_group, WS_DISCONNECTED_BIT | WS_ERROR_BIT,
                           false, false, portMAX_DELAY);
        
        if (!client->should_reconnect) {
            break;
        }
        
        // Check max attempts
        if (client->config.reconnect_max_attempts > 0 &&
            client->reconnect_attempts >= client->config.reconnect_max_attempts) {
            ESP_LOGW(TAG, "Max reconnection attempts reached");
            break;
        }
        
        client->reconnect_attempts++;
        client->state = STREAM_SIGNALING_STATE_RECONNECTING;
        
        ESP_LOGI(TAG, "Reconnecting (attempt %lu)...", client->reconnect_attempts);
        vTaskDelay(pdMS_TO_TICKS(client->config.reconnect_interval_ms));
        
        // Clear event bits
        xEventGroupClearBits(client->event_group, WS_DISCONNECTED_BIT | WS_ERROR_BIT);
        
        // Attempt reconnection
        stream_video_error_t err = stream_signaling_client_connect(client);
        if (err != STREAM_VIDEO_ERR_OK) {
            ESP_LOGE(TAG, "Reconnection failed: %d", err);
        }
    }
    
    vTaskDelete(NULL);
}

/**
 * @brief Get coordinator WebSocket URL (matches Android SDK)
 * 
 * Default coordinator URL: wss://video.stream-io-api.com/video/connect
 * This is used for signaling, not media.
 */
static const char* get_coordinator_ws_url(void)
{
    // Matches Android SDK default coordinator WebSocket URL
    return STREAM_VIDEO_COORDINATOR_WS_URL;
}

stream_video_error_t stream_signaling_client_create(
    const stream_signaling_config_t *config,
    stream_signaling_client_handle_t *client_out)
{
    if (!config || !client_out || !config->api_key) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    stream_signaling_client_handle_t client = (stream_signaling_client_handle_t)calloc(1, sizeof(struct stream_signaling_client));
    if (!client) {
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    // Copy configuration
    memcpy(&client->config, config, sizeof(stream_signaling_config_t));
    client->state = STREAM_SIGNALING_STATE_DISCONNECTED;
    client->should_reconnect = true;
    client->reconnect_attempts = 0;
    client->connection_id[0] = '\0';
    client->rx_buffer = NULL;
    client->rx_size = 0;
    client->rx_offset = 0;
    client->health_task = NULL;
    client->last_event_tick = xTaskGetTickCount();
    
    // Set default reconnect interval if not specified
    if (client->config.reconnect_interval_ms == 0) {
        client->config.reconnect_interval_ms = 5000; // 5 seconds default
    }

    // Use coordinator WebSocket URL (matches Android SDK)
    // Default: wss://video.stream-io-api.com/video/connect
    const char *coordinator_url = config->coordinator_url ? config->coordinator_url : get_coordinator_ws_url();
    const char *query_sep = strchr(coordinator_url, '?') ? "&" : "?";
    int url_len = snprintf(client->signaling_url, sizeof(client->signaling_url),
                           "%s%sapi_key=%s&stream-auth-type=jwt&X-Stream-Client=%s",
                           coordinator_url, query_sep, config->api_key, STREAM_WS_CLIENT_QUERY);
    if (url_len <= 0 || url_len >= (int)sizeof(client->signaling_url)) {
        free(client);
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Using coordinator WebSocket URL: %s", client->signaling_url);

    // Create event group
    client->event_group = xEventGroupCreate();
    if (!client->event_group) {
        free(client);
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    // Configure WebSocket client
    esp_websocket_client_config_t ws_cfg = {
        .uri = client->signaling_url,
        .reconnect_timeout_ms = 0, // We handle reconnection ourselves
        .crt_bundle_attach = esp_crt_bundle_attach,
        .task_stack = 8192,
        .headers = STREAM_WS_HEADERS,
        .ping_interval_sec = 30,
        .pingpong_timeout_sec = 120,
    };

    client->ws_client = esp_websocket_client_init(&ws_cfg);
    if (!client->ws_client) {
        vEventGroupDelete(client->event_group);
        free(client);
        return STREAM_VIDEO_ERR_FAIL;
    }

    // Register event handler
    esp_websocket_register_events(client->ws_client, WEBSOCKET_EVENT_ANY,
                                   websocket_event_handler, client);

    // Create reconnection task
    xTaskCreate(reconnect_task, "signaling_reconnect", 4096, client, 5, &client->reconnect_task);
    xTaskCreate(health_monitor_task, "signaling_health", 4096, client, 5, &client->health_task);

    *client_out = client;
    return STREAM_VIDEO_ERR_OK;
}

void stream_signaling_client_destroy(stream_signaling_client_handle_t client)
{
    if (!client) {
        return;
    }

    // Stop reconnection
    client->should_reconnect = false;
    
    // Disconnect if connected
    if (client->ws_client) {
        esp_websocket_client_stop(client->ws_client);
        esp_websocket_client_destroy(client->ws_client);
    }

    // Clean up task
    if (client->reconnect_task) {
        vTaskDelete(client->reconnect_task);
    }
    if (client->health_task) {
        vTaskDelete(client->health_task);
    }

    // Clean up event group
    if (client->event_group) {
        vEventGroupDelete(client->event_group);
    }

    free(client);
}

stream_video_error_t stream_signaling_client_connect(stream_signaling_client_handle_t client)
{
    if (!client || !client->ws_client) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    if (client->state == STREAM_SIGNALING_STATE_CONNECTED) {
        ESP_LOGW(TAG, "Already connected");
        return STREAM_VIDEO_ERR_INVALID_STATE;
    }

    client->state = STREAM_SIGNALING_STATE_CONNECTING;

    esp_err_t err = esp_websocket_client_start(client->ws_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket client: %s", esp_err_to_name(err));
        client->state = STREAM_SIGNALING_STATE_ERROR;
        return STREAM_VIDEO_ERR_NETWORK;
    }

    return STREAM_VIDEO_ERR_OK;
}

void stream_signaling_client_disconnect(stream_signaling_client_handle_t client)
{
    if (!client || !client->ws_client) {
        return;
    }

    client->should_reconnect = false;
    esp_websocket_client_stop(client->ws_client);
    client->state = STREAM_SIGNALING_STATE_DISCONNECTED;
}

stream_video_error_t stream_signaling_client_send(
    stream_signaling_client_handle_t client,
    const uint8_t *message,
    size_t message_len)
{
    if (!client || !message || message_len == 0) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    if (client->state != STREAM_SIGNALING_STATE_CONNECTED) {
        return STREAM_VIDEO_ERR_INVALID_STATE;
    }

    int len = esp_websocket_client_send_text(
        client->ws_client,
        (const char *)message,
        (int)message_len,
        portMAX_DELAY);
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to send message");
        return STREAM_VIDEO_ERR_NETWORK;
    }

    return STREAM_VIDEO_ERR_OK;
}

stream_signaling_state_t stream_signaling_client_get_state(stream_signaling_client_handle_t client)
{
    if (!client) {
        return STREAM_SIGNALING_STATE_DISCONNECTED;
    }
    return client->state;
}

const char *stream_signaling_client_get_connection_id(stream_signaling_client_handle_t client)
{
    if (!client || !client->connection_id[0]) {
        return NULL;
    }
    return client->connection_id;
}

void stream_signaling_client_set_connection_id(
    stream_signaling_client_handle_t client,
    const char *connection_id)
{
    if (!client) {
        return;
    }
    if (connection_id && connection_id[0]) {
        strncpy(client->connection_id, connection_id, sizeof(client->connection_id) - 1);
        client->connection_id[sizeof(client->connection_id) - 1] = '\0';
    } else {
        client->connection_id[0] = '\0';
    }
}

bool stream_signaling_client_is_connected(stream_signaling_client_handle_t client)
{
    if (!client) {
        return false;
    }
    return client->state == STREAM_SIGNALING_STATE_CONNECTED;
}