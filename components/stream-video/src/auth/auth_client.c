/**
 * @file auth_client.c
 * @brief Authentication client implementation
 * 
 * Implements auth data request matching Android demo app pattern.
 */

#include "stream_video_auth.h"
#include "stream_video_http.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "stream_auth";

// Base URL for auth requests (matches Android demo app)
#define AUTH_BASE_URL "https://pronto.getstream.io/"

/**
 * @brief URL-encode a query component into buffer
 */
static int url_encode_component(const char *input, char *output, size_t output_size)
{
    if (!input || !output || output_size == 0) {
        return -1;
    }

    size_t out_len = 0;
    for (const unsigned char *p = (const unsigned char *)input; *p; ++p) {
        if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
            if (out_len + 1 >= output_size) {
                return -1;
            }
            output[out_len++] = (char)*p;
        } else {
            if (out_len + 3 >= output_size) {
                return -1;
            }
            snprintf(output + out_len, output_size - out_len, "%%%02X", *p);
            out_len += 3;
        }
    }

    if (out_len >= output_size) {
        return -1;
    }
    output[out_len] = '\0';
    return (int)out_len;
}

/**
 * @brief HTTP event handler context
 */
typedef struct {
    stream_video_auth_callback_t callback;
    void *user_data;
    char *response_buffer;
    size_t response_size;
    size_t response_offset;
} http_event_context_t;

/**
 * @brief HTTP event handler for auth request
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_event_context_t *ctx = (http_event_context_t *)evt->user_data;
    
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
            
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            if (ctx) {
                ctx->response_offset = 0;
            }
            break;
            
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
            
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
            
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client) && ctx) {
                ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
                
                // Accumulate response data
                if (ctx->response_offset + evt->data_len < ctx->response_size) {
                    memcpy(ctx->response_buffer + ctx->response_offset, evt->data, evt->data_len);
                    ctx->response_offset += evt->data_len;
                    ctx->response_buffer[ctx->response_offset] = '\0';
                }
            }
            break;
            
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (ctx && ctx->response_offset > 0) {
                // Parse JSON response
                cJSON *json = cJSON_Parse(ctx->response_buffer);
                if (json) {
                    stream_video_auth_data_t auth_data = {0};
                    
                    // Parse GetAuthDataResponse: { userId, apiKey, token }
                    cJSON *userId_item = cJSON_GetObjectItem(json, "userId");
                    cJSON *apiKey_item = cJSON_GetObjectItem(json, "apiKey");
                    cJSON *token_item = cJSON_GetObjectItem(json, "token");
                    
                    if (userId_item && cJSON_IsString(userId_item)) {
                        strncpy(auth_data.user_id, userId_item->valuestring, sizeof(auth_data.user_id) - 1);
                    }
                    if (apiKey_item && cJSON_IsString(apiKey_item)) {
                        strncpy(auth_data.api_key, apiKey_item->valuestring, sizeof(auth_data.api_key) - 1);
                    }
                    if (token_item && cJSON_IsString(token_item)) {
                        strncpy(auth_data.token, token_item->valuestring, sizeof(auth_data.token) - 1);
                    }
                    
                    // Call callback if all fields are present
                    if (auth_data.user_id[0] && auth_data.api_key[0] && auth_data.token[0]) {
                        if (ctx->callback) {
                            ctx->callback(&auth_data, ctx->user_data);
                        }
                    } else {
                        ESP_LOGE(TAG, "Missing fields in auth response");
                    }
                    
                    cJSON_Delete(json);
                } else {
                    ESP_LOGE(TAG, "Failed to parse JSON response");
                }
            }
            break;
            
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
            
        default:
            break;
    }
    return ESP_OK;
}

stream_video_error_t stream_video_request_auth_data(
    const stream_video_auth_request_t *request,
    stream_video_auth_callback_t callback,
    void *user_data)
{
    if (!request || !request->environment) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    // Build URL: https://pronto.getstream.io/api/auth/create-token?environment=xxx&user_id=xxx&exp=xxx
    char url[512];
    int url_len = snprintf(url, sizeof(url), "%sapi/auth/create-token?environment=%s&exp=%lu",
                           AUTH_BASE_URL, request->environment, request->exp);
    
    if (request->user_id) {
        char encoded_user_id[192];
        if (url_encode_component(request->user_id, encoded_user_id, sizeof(encoded_user_id)) < 0) {
            ESP_LOGE(TAG, "Failed to encode user_id");
            return STREAM_VIDEO_ERR_INVALID_ARG;
        }
        url_len += snprintf(url + url_len, sizeof(url) - url_len, "&user_id=%s", encoded_user_id);
    }
    
    if (url_len >= sizeof(url)) {
        ESP_LOGE(TAG, "URL too long");
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Requesting auth data from: %s", url);

    // Allocate response buffer
    char *response_buffer = (char *)malloc(2048);
    if (!response_buffer) {
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    // Create event context
    http_event_context_t ctx = {
        .callback = callback,
        .user_data = user_data,
        .response_buffer = response_buffer,
        .response_size = 2048,
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

    // Perform GET request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d", status_code);
        
        if (status_code != 200) {
            ESP_LOGE(TAG, "HTTP request failed with status %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(response_buffer);

    return (err == ESP_OK) ? STREAM_VIDEO_ERR_OK : STREAM_VIDEO_ERR_NETWORK;
}

stream_video_error_t stream_video_create_user_from_auth(
    const stream_video_auth_data_t *auth_data,
    stream_video_user_role_t role,
    stream_video_user_t *user_out)
{
    if (!auth_data || !user_out || !auth_data->user_id[0]) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    // Create user object with id and role (minimum required)
    // Matches Android SDK: User(id = authData.userId, role = "admin")
    user_out->id = auth_data->user_id;
    user_out->role = role;
    user_out->name = NULL;      // Optional
    user_out->image = NULL;     // Optional

    ESP_LOGI(TAG, "Created user: id=%s, role=%d", user_out->id, user_out->role);

    return STREAM_VIDEO_ERR_OK;
}

