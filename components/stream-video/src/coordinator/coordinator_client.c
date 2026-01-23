/**
 * @file coordinator_client.c
 * @brief Coordinator WebSocket and REST API client
 * 
 * Implements coordinator connection and joinCall REST API.
 * Matches Android SDK pattern.
 */

#include "stream_video_coordinator.h"
#include "stream_video_http.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <strings.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "stream_coordinator";
static const char *HINT_URL = "https://hint.stream-io-video.com/";

typedef struct {
    char location[8];
} location_hint_context_t;

static esp_err_t hint_event_handler(esp_http_client_event_t *evt)
{
    location_hint_context_t *ctx = (location_hint_context_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_HEADER && ctx && evt->header_key && evt->header_value) {
        if (strcasecmp(evt->header_key, "X-Amz-Cf-Pop") == 0) {
            strncpy(ctx->location, evt->header_value, 3);
            ctx->location[3] = '\0';
        }
    }

    return ESP_OK;
}

static const char *select_location(void)
{
    // Keep enough room for default fallback string + null terminator.
    static char location[32] = "missing-location";
    location_hint_context_t ctx = {0};

    esp_http_client_handle_t client = stream_video_http_client_init(
        HINT_URL,
        hint_event_handler,
        &ctx);
    if (!client) {
        return location;
    }

    esp_http_client_set_method(client, HTTP_METHOD_HEAD);
    if (esp_http_client_perform(client) == ESP_OK) {
        if (ctx.location[0]) {
            strncpy(location, ctx.location, sizeof(location) - 1);
            location[sizeof(location) - 1] = '\0';
        }
    }

    esp_http_client_cleanup(client);
    return location;
}

/**
 * @brief HTTP event handler context for joinCall
 */
typedef struct {
    stream_video_join_call_callback_t callback;
    void *user_data;
    char *response_buffer;
    size_t response_size;
    size_t response_offset;
} http_event_context_t;

static const char *get_string_field(cJSON *object, const char *primary_key, const char *fallback_key)
{
    if (!object) {
        return NULL;
    }
    cJSON *item = cJSON_GetObjectItem(object, primary_key);
    if (item && cJSON_IsString(item)) {
        return item->valuestring;
    }
    if (fallback_key) {
        item = cJSON_GetObjectItem(object, fallback_key);
        if (item && cJSON_IsString(item)) {
            return item->valuestring;
        }
    }
    return NULL;
}

/**
 * @brief HTTP event handler for joinCall request
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_event_context_t *ctx = (http_event_context_t *)evt->user_data;
    
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (ctx) {
                // Accumulate response data (keep buffer null-terminated)
                if (ctx->response_offset + evt->data_len < ctx->response_size) {
                    memcpy(ctx->response_buffer + ctx->response_offset, evt->data, evt->data_len);
                    ctx->response_offset += evt->data_len;
                    ctx->response_buffer[ctx->response_offset] = '\0';
                } else {
                    ESP_LOGW(TAG, "Join call response truncated (buffer too small)");
                }
            }
            break;
            
        case HTTP_EVENT_ON_FINISH:
            if (ctx && ctx->response_offset > 0) {
                // Parse JSON response
                cJSON *json = cJSON_Parse(ctx->response_buffer);
                if (json) {
                    stream_video_join_call_response_t response = {0};
                    
                    // Parse JoinCallResponse structure
                    // Response format matches Android SDK
                    cJSON *call_item = cJSON_GetObjectItem(json, "call");
                    if (call_item) {
                        cJSON *call_id_item = cJSON_GetObjectItem(call_item, "id");
                        if (call_id_item && cJSON_IsString(call_id_item)) {
                            strncpy(response.call_id, call_id_item->valuestring, sizeof(response.call_id) - 1);
                        }
                        cJSON *session_id_item = cJSON_GetObjectItem(call_item, "current_session_id");
                        if (session_id_item && cJSON_IsString(session_id_item)) {
                            strncpy(response.session_id, session_id_item->valuestring,
                                   sizeof(response.session_id) - 1);
                        }
                    }
                    
                    // Parse credentials.server.wsEndpoint
                    cJSON *credentials_item = cJSON_GetObjectItem(json, "credentials");
                    if (credentials_item) {
                        cJSON *server_item = cJSON_GetObjectItem(credentials_item, "server");
                        if (server_item) {
                            const char *ws_endpoint =
                                get_string_field(server_item, "wsEndpoint", "ws_endpoint");
                            const char *http_url =
                                get_string_field(server_item, "url", NULL);

                            if (ws_endpoint) {
                                strncpy(response.credentials.ws_endpoint,
                                       ws_endpoint,
                                       sizeof(response.credentials.ws_endpoint) - 1);
                            }
                            if (http_url) {
                                strncpy(response.credentials.http_url,
                                       http_url,
                                       sizeof(response.credentials.http_url) - 1);
                            }
                        }
                        
                        // Parse credentials.token
                        const char *token =
                            get_string_field(credentials_item, "token", NULL);
                        if (token) {
                            strncpy(response.credentials.token,
                                   token,
                                   sizeof(response.credentials.token) - 1);
                        }
                        
                        // Parse credentials.iceServers
                        cJSON *ice_servers = cJSON_GetObjectItem(credentials_item, "iceServers");
                        if (!ice_servers) {
                            ice_servers = cJSON_GetObjectItem(credentials_item, "ice_servers");
                        }
                        if (ice_servers) {
                            char *ice_servers_str = cJSON_Print(ice_servers);
                            if (ice_servers_str) {
                                strncpy(response.credentials.ice_servers, 
                                       ice_servers_str, 
                                       sizeof(response.credentials.ice_servers) - 1);
                                free(ice_servers_str);
                            }
                        }
                    }
                    
                    // Check if successful
                    response.success = (response.credentials.ws_endpoint[0] != '\0' && 
                                      response.credentials.token[0] != '\0');
                    
                    // Call callback
                    if (ctx->callback) {
                        ctx->callback(&response, ctx->user_data);
                    }
                    
                    cJSON_Delete(json);
                } else {
                    ESP_LOGE(TAG, "Failed to parse joinCall response: %s", ctx->response_buffer);
                }
            } else {
                ESP_LOGW(TAG, "Join call response empty");
            }
            break;
            
        default:
            break;
    }
    return ESP_OK;
}

stream_video_error_t stream_video_coordinator_join_call(
    const stream_video_join_call_request_t *request,
    stream_video_join_call_callback_t callback,
    void *user_data)
{
    if (!request || !request->api_key || !request->user_id || !request->token || !request->call_type) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    // Build URL: https://video.stream-io-api.com/video/calls/{call_type}/{call_id}/join
    // Or: https://video.stream-io-api.com/video/calls/{call_type}/join (if call_id is NULL)
    char url[512];
    int url_len;
    
    if (request->call_id) {
        url_len = snprintf(url, sizeof(url), 
                          "%s/video/call/%s/%s/join?api_key=%s",
                          STREAM_VIDEO_COORDINATOR_REST_URL,
                          request->call_type,
                          request->call_id,
                          request->api_key);
    } else {
        url_len = snprintf(url, sizeof(url), 
                          "%s/video/call/%s/join?api_key=%s",
                          STREAM_VIDEO_COORDINATOR_REST_URL,
                          request->call_type,
                          request->api_key);
    }

    if (request->connection_id && request->connection_id[0]) {
        url_len += snprintf(url + url_len, sizeof(url) - url_len,
                            "&connection_id=%s", request->connection_id);
    }
    
    if (url_len >= sizeof(url)) {
        ESP_LOGE(TAG, "URL too long");
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Joining call: %s", url);

    // Allocate response buffer
    char *response_buffer = (char *)calloc(1, 32768);
    if (!response_buffer) {
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    // Create event context
    http_event_context_t ctx = {
        .callback = callback,
        .user_data = user_data,
        .response_buffer = response_buffer,
        .response_size = 32768,
        .response_offset = 0,
    };

    // Configure HTTP client
    esp_http_client_handle_t client = stream_video_http_client_init(
        url,
        http_event_handler,
        &ctx);
    if (!client) {
        free(response_buffer);
        return STREAM_VIDEO_ERR_FAIL;
    }

    // Set headers
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Authorization", request->token);
    esp_http_client_set_header(client, "stream-auth-type", "jwt");
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // Set POST body
    const char *location = request->location ? request->location : select_location();

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "user_id", request->user_id);
    cJSON_AddStringToObject(body, "location", location);
    if (request->create) {
        cJSON_AddTrueToObject(body, "create");
    }
    char *body_str = cJSON_Print(body);
    cJSON_Delete(body);
    
    if (body_str) {
        esp_http_client_set_post_field(client, body_str, strlen(body_str));
    }

    // Perform POST request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Join call HTTP Status = %d", status_code);

        if (status_code < 200 || status_code >= 300) {
            if (ctx.response_offset > 0) {
                ESP_LOGE(TAG, "Join call failed (%d): %s", status_code, ctx.response_buffer);
            } else {
                ESP_LOGE(TAG, "Join call failed with status %d", status_code);
            }
            err = ESP_FAIL;
        } else if (ctx.response_offset > 0) {
            ESP_LOGI(TAG, "Join call response: %s", ctx.response_buffer);
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    if (body_str) {
        free(body_str);
    }
    esp_http_client_cleanup(client);
    free(response_buffer);

    return (err == ESP_OK) ? STREAM_VIDEO_ERR_OK : STREAM_VIDEO_ERR_NETWORK;
}

