/**
 * @file capture_internal.h
 * @brief Internal capture functions — called by the SDK, not exposed to users.
 */

#ifndef STREAM_VIDEO_CAPTURE_INTERNAL_H
#define STREAM_VIDEO_CAPTURE_INTERNAL_H

#include "stream_video.h"
#include "esp_capture_sink.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

stream_video_error_t stream_video_default_capture_prepare(
    void *user_data,
    esp_capture_sink_handle_t *sink_out);

void stream_video_default_capture_stop(void *user_data);

esp_err_t stream_video_default_capture_run_resolution_test(void);

#ifdef __cplusplus
}
#endif

#endif
