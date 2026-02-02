#pragma once

#include "esp_err.h"
#include "stream_video.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t media_publish_start(stream_video_client_handle_t client);
esp_err_t media_publish_stop(stream_video_client_handle_t client);

#ifdef __cplusplus
}
#endif
