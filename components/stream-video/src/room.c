/**
 * @file room.c
 * @brief Room management implementation
 */

#include "stream_video.h"
#include "stream_video_auth.h"
#include "stream_video_coordinator.h"
#include "stream_video_signaling.h"
#include "stream_video_sfu.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char *TAG = "stream_video_room";

typedef struct stream_video_client {
    stream_video_auth_data_t auth_data;
    stream_signaling_client_handle_t coordinator_client;
    stream_sfu_client_handle_t sfu_client;
    stream_video_join_call_params_t params;
    EventGroupHandle_t event_group;
    TaskHandle_t join_task;
} stream_video_client_t;

// Event bits
#define JOIN_AUTH_DONE_BIT    BIT0
#define JOIN_DONE_BIT         BIT1
#define JOIN_FAILED_BIT       BIT2

static size_t parse_ice_servers(const char *ice_json, esp_peer_ice_server_cfg_t **servers_out)
{
    if (!ice_json || !servers_out) {
        return 0;
    }

    cJSON *root = cJSON_Parse(ice_json);
    if (!root || !cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return 0;
    }

    size_t count = (size_t)cJSON_GetArraySize(root);
    if (count == 0) {
        cJSON_Delete(root);
        return 0;
    }

    esp_peer_ice_server_cfg_t *servers =
        (esp_peer_ice_server_cfg_t *)calloc(count, sizeof(esp_peer_ice_server_cfg_t));
    if (!servers) {
        cJSON_Delete(root);
        return 0;
    }

    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        cJSON *item = cJSON_GetArrayItem(root, (int)i);
        if (!item) {
            continue;
        }
        cJSON *urls = cJSON_GetObjectItem(item, "urls");
        cJSON *username = cJSON_GetObjectItem(item, "username");
        cJSON *password = cJSON_GetObjectItem(item, "password");
        const char *url_str = NULL;

        if (urls && cJSON_IsArray(urls) && cJSON_GetArraySize(urls) > 0) {
            cJSON *first_url = cJSON_GetArrayItem(urls, 0);
            if (first_url && cJSON_IsString(first_url)) {
                url_str = first_url->valuestring;
            }
        }

        if (!url_str || !username || !password || !cJSON_IsString(username) || !cJSON_IsString(password)) {
            continue;
        }

        servers[used].stun_url = strdup(url_str);
        servers[used].user = strdup(username->valuestring);
        servers[used].psw = strdup(password->valuestring);
        used++;
    }

    cJSON_Delete(root);
    if (used == 0) {
        free(servers);
        return 0;
    }

    *servers_out = servers;
    return used;
}

static void publish_join_result(stream_video_client_t *client,
                                bool success,
                                const char *error_message)
{
    if (!client || !client->params.result_cb) {
        return;
    }

    stream_video_join_result_t result = {
        .success = success
    };

    if (error_message) {
        strncpy(result.error_message, error_message, sizeof(result.error_message) - 1);
    }

    client->params.result_cb(&result, client->params.user_data);
}

static void on_sfu_event(stream_sfu_client_handle_t client,
                         const stream_sfu_event_t *event,
                         void *user_data)
{
    (void)client;
    (void)user_data;

    if (!event) {
        return;
    }

    switch (event->type) {
        case STREAM_SFU_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✓ Connected to SFU WebSocket");
            ESP_LOGI(TAG, "Ready for WebRTC negotiation");
            if (user_data) {
                stream_video_client_t *client = (stream_video_client_t *)user_data;
                if (client->params.sfu_connected_cb) {
                    client->params.sfu_connected_cb(client, client->params.sfu_user_data);
                }
            }
            break;
        case STREAM_SFU_EVENT_MESSAGE_RECEIVED:
            ESP_LOGI(TAG, "SFU message received (SDP/ICE)");
            break;
        case STREAM_SFU_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "SFU WebSocket disconnected");
            break;
        case STREAM_SFU_EVENT_ERROR:
            ESP_LOGE(TAG, "SFU WebSocket error");
            break;
        default:
            break;
    }
}

static stream_video_error_t on_join_call_response(
    const stream_video_join_call_response_t *response,
    void *user_data)
{
    stream_video_client_t *client = (stream_video_client_t *)user_data;
    if (!client || !response) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    if (response->success) {
        ESP_LOGI(TAG, "✓ Join call successful");
        ESP_LOGI(TAG, "  Call ID: %s", response->call_id);
        ESP_LOGI(TAG, "  SFU WebSocket: %s", response->credentials.ws_endpoint);

        stream_sfu_config_t sfu_config = {
            .ws_endpoint = response->credentials.ws_endpoint,
            .token = response->credentials.token,
            .event_cb = on_sfu_event,
            .user_data = client,
        };

        stream_video_error_t err = stream_sfu_client_create(&sfu_config, &client->sfu_client);
        if (err != STREAM_VIDEO_ERR_OK) {
            ESP_LOGE(TAG, "Failed to create SFU client: %d", err);
            xEventGroupSetBits(client->event_group, JOIN_FAILED_BIT);
            return err;
        }

        if (response->credentials.http_url[0]) {
            stream_sfu_client_set_http_url(client->sfu_client, response->credentials.http_url);
        }

        if (response->credentials.ice_servers[0]) {
            esp_peer_ice_server_cfg_t *servers = NULL;
            size_t server_count = parse_ice_servers(response->credentials.ice_servers, &servers);
            if (server_count > 0 && servers) {
#if defined(CONFIG_STREAM_VIDEO_STUN_OVERRIDE)
                if (CONFIG_STREAM_VIDEO_STUN_OVERRIDE[0] != '\0') {
                    for (size_t i = 0; i < server_count; ++i) {
                        free(servers[i].stun_url);
                        free(servers[i].user);
                        free(servers[i].psw);
                    }
                    free(servers);
                    servers = (esp_peer_ice_server_cfg_t *)calloc(1, sizeof(esp_peer_ice_server_cfg_t));
                    if (servers) {
                        servers[0].stun_url = strdup(CONFIG_STREAM_VIDEO_STUN_OVERRIDE);
                        servers[0].user = strdup("");
                        servers[0].psw = strdup("");
                        server_count = 1;
                        ESP_LOGI(TAG, "Using STUN override: %s", CONFIG_STREAM_VIDEO_STUN_OVERRIDE);
                    } else {
                        server_count = 0;
                        ESP_LOGE(TAG, "Failed to allocate STUN override config");
                    }
                }
#endif
                for (size_t i = 0; i < server_count; ++i) {
                    ESP_LOGI(TAG, "ICE server[%u]: url=%s user_len=%u",
                             (unsigned)i,
                             servers[i].stun_url ? servers[i].stun_url : "(null)",
                             servers[i].user ? (unsigned)strlen(servers[i].user) : 0u);
                }
                stream_sfu_client_set_ice_servers(client->sfu_client, servers, server_count);
                for (size_t i = 0; i < server_count; ++i) {
                    free(servers[i].stun_url);
                    free(servers[i].user);
                    free(servers[i].psw);
                }
                free(servers);
            }
        }

        err = stream_sfu_client_connect(client->sfu_client);
        if (err != STREAM_VIDEO_ERR_OK) {
            ESP_LOGE(TAG, "Failed to connect to SFU: %d", err);
            publish_join_result(client, false,
                                "Failed to connect to SFU");
            xEventGroupSetBits(client->event_group, JOIN_FAILED_BIT);
            return err;
        }

        if (response->session_id[0]) {
            err = stream_sfu_client_set_join_info(
                client->sfu_client,
                response->credentials.token,
                response->session_id);
            if (err != STREAM_VIDEO_ERR_OK) {
                ESP_LOGE(TAG, "Failed to send SFU join request: %d", err);
            } else {
                ESP_LOGI(TAG, "✓ SFU join request queued");
            }
        }

        publish_join_result(client, true, NULL);
        xEventGroupSetBits(client->event_group, JOIN_DONE_BIT);
    } else {
        ESP_LOGE(TAG, "Join call failed: %s", response->error_message);
        publish_join_result(client, false, response->error_message);
        xEventGroupSetBits(client->event_group, JOIN_FAILED_BIT);
    }

    return STREAM_VIDEO_ERR_OK;
}

static void on_coordinator_event(stream_signaling_client_handle_t client_handle,
                                 const stream_signaling_event_t *event,
                                 void *user_data)
{
    stream_video_client_t *client = (stream_video_client_t *)user_data;
    if (!client || !event) {
        return;
    }

    switch (event->type) {
        case STREAM_SIGNALING_EVENT_CONNECTED: {
            uint8_t *message = NULL;
            size_t message_len = 0;

            stream_video_error_t err = stream_signaling_create_connect_message(
                client->auth_data.user_id,
                client->auth_data.token,
                NULL,
                &message,
                &message_len);

            if (err == STREAM_VIDEO_ERR_OK && message) {
                ESP_LOGI(TAG, "Sending connect message to coordinator...");
                stream_signaling_client_send(client_handle, message, message_len);
                free(message);
            } else {
                ESP_LOGE(TAG, "Failed to create connect message: %d", err);
            }
            break;
        }
        case STREAM_SIGNALING_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Coordinator WebSocket disconnected");
            break;
        case STREAM_SIGNALING_EVENT_ERROR:
            ESP_LOGE(TAG, "Coordinator WebSocket error");
            break;
        default:
            break;
    }
}

static stream_video_error_t on_auth_data_received(
    const stream_video_auth_data_t *auth_data,
    void *user_data)
{
    stream_video_client_t *client = (stream_video_client_t *)user_data;
    if (!client || !auth_data) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    memcpy(&client->auth_data, auth_data, sizeof(stream_video_auth_data_t));
    xEventGroupSetBits(client->event_group, JOIN_AUTH_DONE_BIT);
    return STREAM_VIDEO_ERR_OK;
}

static void join_flow_task(void *arg)
{
    stream_video_client_t *client = (stream_video_client_t *)arg;
    if (!client) {
        vTaskDelete(NULL);
    }

    stream_video_auth_request_t auth_req = {
        .environment = client->params.environment,
        .user_id = client->params.user_id,
        .exp = client->params.exp,
    };

    stream_video_error_t err = stream_video_request_auth_data(
        &auth_req,
        on_auth_data_received,
        client);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to request auth data: %d", err);
        publish_join_result(client, false, "Failed to request auth data");
        xEventGroupSetBits(client->event_group, JOIN_FAILED_BIT);
        vTaskDelete(NULL);
    }

    EventBits_t bits = xEventGroupWaitBits(
        client->event_group,
        JOIN_AUTH_DONE_BIT,
        pdTRUE,
        pdTRUE,
        pdMS_TO_TICKS(30000));
    if ((bits & JOIN_AUTH_DONE_BIT) == 0) {
        ESP_LOGE(TAG, "Auth timed out");
        publish_join_result(client, false, "Auth timed out");
        xEventGroupSetBits(client->event_group, JOIN_FAILED_BIT);
        vTaskDelete(NULL);
    }

    stream_signaling_config_t coordinator_config = {
        .api_key = client->auth_data.api_key,
        .user_id = client->auth_data.user_id,
        .token = client->auth_data.token,
        .coordinator_url = NULL,
        .event_cb = on_coordinator_event,
        .user_data = client,
        .reconnect_interval_ms = 5000,
        .reconnect_max_attempts = 10,
    };

    err = stream_signaling_client_create(&coordinator_config, &client->coordinator_client);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to create coordinator client: %d", err);
        publish_join_result(client, false, "Failed to create coordinator client");
        xEventGroupSetBits(client->event_group, JOIN_FAILED_BIT);
        vTaskDelete(NULL);
    }

    err = stream_signaling_client_connect(client->coordinator_client);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to connect to coordinator: %d", err);
        publish_join_result(client, false, "Failed to connect to coordinator");
        xEventGroupSetBits(client->event_group, JOIN_FAILED_BIT);
        vTaskDelete(NULL);
    }

    const char *connection_id = NULL;
    for (int i = 0; i < 20; ++i) {
        connection_id = stream_signaling_client_get_connection_id(client->coordinator_client);
        if (connection_id) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (!connection_id) {
        ESP_LOGE(TAG, "No connection_id from coordinator; aborting join");
        xEventGroupSetBits(client->event_group, JOIN_FAILED_BIT);
        vTaskDelete(NULL);
    }

    stream_video_join_call_request_t join_req = {
        .api_key = client->auth_data.api_key,
        .user_id = client->auth_data.user_id,
        .token = client->auth_data.token,
        .call_type = client->params.call_type,
        .call_id = client->params.call_id,
        .create = client->params.create,
        .location = client->params.location,
        .connection_id = connection_id,
    };

    stream_video_coordinator_join_call(&join_req, on_join_call_response, client);

    vTaskDelete(NULL);
}

stream_video_error_t stream_video_init(void)
{
    ESP_LOGI(TAG, "Stream Video SDK initialized");
    return STREAM_VIDEO_ERR_OK;
}

void stream_video_deinit(void)
{
    ESP_LOGI(TAG, "Stream Video SDK deinitialized");
}

stream_video_error_t stream_video_join_call(
    const stream_video_join_call_params_t *params,
    stream_video_client_handle_t *client_out)
{
    if (!params || !params->environment || !params->call_type || !client_out) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    stream_video_client_t *client = (stream_video_client_t *)calloc(1, sizeof(stream_video_client_t));
    if (!client) {
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    client->params = *params;
    client->event_group = xEventGroupCreate();
    if (!client->event_group) {
        free(client);
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreate(
        join_flow_task,
        "stream_join",
        CONFIG_STREAM_VIDEO_JOIN_TASK_STACK,
        client,
        5,
        &client->join_task);
    if (ok != pdPASS) {
        vEventGroupDelete(client->event_group);
        free(client);
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    *client_out = client;
    return STREAM_VIDEO_ERR_OK;
}

stream_video_error_t stream_video_leave_call(stream_video_client_handle_t client_handle)
{
    stream_video_client_t *client = (stream_video_client_t *)client_handle;
    if (!client) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    if (client->sfu_client) {
        stream_sfu_client_destroy(client->sfu_client);
        client->sfu_client = NULL;
    }
    if (client->coordinator_client) {
        stream_signaling_client_destroy(client->coordinator_client);
        client->coordinator_client = NULL;
    }
    if (client->event_group) {
        vEventGroupDelete(client->event_group);
        client->event_group = NULL;
    }
    free(client);

    return STREAM_VIDEO_ERR_OK;
}

stream_video_error_t stream_video_start_publishing(
    stream_video_client_handle_t client_handle,
    const stream_video_publish_params_t *params)
{
    stream_video_client_t *client = (stream_video_client_t *)client_handle;
    if (!client || !params || !client->sfu_client || !params->sink) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }
    return stream_sfu_client_start_publishing(
        client->sfu_client,
        params->sink,
        params->publish_audio,
        params->publish_video);
}

stream_video_error_t stream_video_stop_publishing(stream_video_client_handle_t client_handle)
{
    stream_video_client_t *client = (stream_video_client_t *)client_handle;
    if (!client || !client->sfu_client) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }
    return stream_sfu_client_stop_publishing(client->sfu_client);
}

