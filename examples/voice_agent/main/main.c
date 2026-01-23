/**
 * @file main.c
 * @brief Voice agent example - AI agent interaction
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "stream_video.h"

// Forward declaration - will be in stream_video.h later
extern const char* stream_video_error_to_string(stream_video_error_t error);

static const char *TAG = "voice_agent_example";

void app_main(void)
{
    ESP_LOGI(TAG, "Stream Video ESP32 - Voice Agent Example");

    // Initialize Stream Video SDK
    stream_video_error_t err = stream_video_init();
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to initialize Stream Video SDK: %s", 
                 stream_video_error_to_string(err));
        return;
    }

    ESP_LOGI(TAG, "Stream Video SDK initialized successfully");

    // TODO: Add example code for AI agent interaction

    // Keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

