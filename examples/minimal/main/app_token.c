/**
 * @file app_token.c
 * @brief App-side token request implementation
 *
 * The app fetches the token from its own backend or Stream's token service.
 * This is example code; replace with your own endpoint if needed.
 */

#include "app_token.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "app_token";

static int url_encode_component(const char *input, char *output, size_t output_size)
{
    if (!input || !output || output_size == 0) return -1;
    size_t out_len = 0;
    for (const unsigned char *p = (const unsigned char *)input; *p; ++p) {
        if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
            if (out_len + 1 >= output_size) return -1;
            output[out_len++] = (char)*p;
        } else {
            if (out_len + 3 >= output_size) return -1;
            snprintf(output + out_len, output_size - out_len, "%%%02X", *p);
            out_len += 3;
        }
    }
    if (out_len >= output_size) return -1;
    output[out_len] = '\0';
    return (int)out_len;
}

typedef struct {
    char *buffer;
    size_t size;
    size_t offset;
} http_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_ctx_t *ctx = (http_ctx_t *)evt->user_data;
    if (!ctx) return ESP_OK;
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client) && ctx->buffer) {
                if (ctx->offset + evt->data_len < ctx->size) {
                    memcpy(ctx->buffer + ctx->offset, evt->data, evt->data_len);
                    ctx->offset += evt->data_len;
                    ctx->buffer[ctx->offset] = '\0';
                }
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

stream_video_error_t app_request_auth_data(
    const char *base_url,
    const char *environment,
    const char *user_id,
    uint32_t exp,
    stream_video_auth_data_t *auth_data_out)
{
    if (!base_url || !environment || !auth_data_out) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    memset(auth_data_out, 0, sizeof(stream_video_auth_data_t));

    char url[512];
    int n = snprintf(url, sizeof(url), "%sapi/auth/create-token?environment=%s&exp=%lu",
                     base_url, environment, (unsigned long)exp);
    if (n < 0 || n >= (int)sizeof(url)) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }
    if (user_id && user_id[0]) {
        char encoded[192];
        if (url_encode_component(user_id, encoded, sizeof(encoded)) < 0) return STREAM_VIDEO_ERR_INVALID_ARG;
        n += snprintf(url + n, sizeof(url) - n, "&user_id=%s", encoded);
        if (n >= (int)sizeof(url)) return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Requesting token from token service...");

    char *buffer = (char *)malloc(2048);
    if (!buffer) return STREAM_VIDEO_ERR_NO_MEM;

    http_ctx_t ctx = { .buffer = buffer, .size = 2048, .offset = 0 };

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(buffer);
        return STREAM_VIDEO_ERR_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    stream_video_error_t ret = STREAM_VIDEO_ERR_NETWORK;
    if (err == ESP_OK && esp_http_client_get_status_code(client) == 200 && ctx.offset > 0) {
        cJSON *json = cJSON_Parse(buffer);
        if (json) {
            cJSON *userId_item = cJSON_GetObjectItem(json, "userId");
            cJSON *apiKey_item = cJSON_GetObjectItem(json, "apiKey");
            cJSON *token_item = cJSON_GetObjectItem(json, "token");
            if (userId_item && cJSON_IsString(userId_item) &&
                apiKey_item && cJSON_IsString(apiKey_item) &&
                token_item && cJSON_IsString(token_item)) {
                strncpy(auth_data_out->user_id, userId_item->valuestring, sizeof(auth_data_out->user_id) - 1);
                strncpy(auth_data_out->api_key, apiKey_item->valuestring, sizeof(auth_data_out->api_key) - 1);
                strncpy(auth_data_out->token, token_item->valuestring, sizeof(auth_data_out->token) - 1);
                ret = STREAM_VIDEO_ERR_OK;
                ESP_LOGI(TAG, "Token received for user %s", auth_data_out->user_id);
            } else {
                ESP_LOGE(TAG, "Token response missing required fields");
            }
            cJSON_Delete(json);
        } else {
            ESP_LOGE(TAG, "Failed to parse token response JSON");
        }
    } else {
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGE(TAG, "HTTP status %d", esp_http_client_get_status_code(client));
        }
    }

    esp_http_client_cleanup(client);
    free(buffer);
    return ret;
}
