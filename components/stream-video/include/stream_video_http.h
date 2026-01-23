#ifndef STREAM_VIDEO_HTTP_H
#define STREAM_VIDEO_HTTP_H

#include "esp_http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize an HTTP client with Stream defaults
 *
 * This sets the certificate bundle for TLS verification.
 */
esp_http_client_handle_t stream_video_http_client_init(
    const char *url,
    esp_err_t (*event_handler)(esp_http_client_event_t *evt),
    void *user_data);

#ifdef __cplusplus
}
#endif

#endif // STREAM_VIDEO_HTTP_H
