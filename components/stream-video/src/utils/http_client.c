/**
 * @file http_client.c
 * @brief Shared HTTP client helpers
 */

#include "stream_video_http.h"
#include "esp_crt_bundle.h"

esp_http_client_handle_t stream_video_http_client_init(
    const char *url,
    esp_err_t (*event_handler)(esp_http_client_event_t *evt),
    void *user_data)
{
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = event_handler,
        .user_data = user_data,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
        .timeout_ms = 15000,
    };

    return esp_http_client_init(&config);
}
