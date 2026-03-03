/**
 * @file stream_video_capture.h
 * @brief Default capture implementation for Stream Video SDK (ESP32-S3 / ESP32-P4)
 *
 * Use this when you want the SDK to handle capture preparation via the
 * capture provider. Register with stream_video_set_capture_provider().
 */

#ifndef STREAM_VIDEO_CAPTURE_H
#define STREAM_VIDEO_CAPTURE_H

#include "stream_video.h"
#include "esp_capture_sink.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Prepare capture and return sink (for use as capture provider prepare_cb)
 * @param user_data Unused; pass NULL
 * @param sink_out  On success, set to the capture sink handle
 * @return stream_video_error_t STREAM_VIDEO_ERR_OK on success
 */
stream_video_error_t stream_video_default_capture_prepare(
    void *user_data,
    esp_capture_sink_handle_t *sink_out);

/**
 * @brief Stop capture and release resources (for use as capture provider stop_cb)
 * @param user_data Unused; pass NULL
 */
void stream_video_default_capture_stop(void *user_data);

/**
 * @brief Run a resolution probe test (optional; for diagnostics)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t stream_video_default_capture_run_resolution_test(void);

#ifdef __cplusplus
}
#endif

#endif /* STREAM_VIDEO_CAPTURE_H */
