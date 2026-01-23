/**
 * @file error_handler.c
 * @brief Error handling utilities
 */

#include "stream_video.h"
#include "esp_log.h"

// TAG unused for now - comment out to avoid warning
// static const char *TAG = "stream_error";

/**
 * @brief Get error string from error code
 */
const char* stream_video_error_to_string(stream_video_error_t error)
{
    switch (error) {
        case STREAM_VIDEO_ERR_OK:
            return "OK";
        case STREAM_VIDEO_ERR_INVALID_ARG:
            return "Invalid argument";
        case STREAM_VIDEO_ERR_NO_MEM:
            return "Out of memory";
        case STREAM_VIDEO_ERR_INVALID_STATE:
            return "Invalid state";
        case STREAM_VIDEO_ERR_NETWORK:
            return "Network error";
        case STREAM_VIDEO_ERR_TIMEOUT:
            return "Timeout";
        case STREAM_VIDEO_ERR_FAIL:
            return "General failure";
        default:
            return "Unknown error";
    }
}


