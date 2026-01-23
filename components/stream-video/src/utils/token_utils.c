/**
 * @file token_utils.c
 * @brief Token generation and management utilities
 * 
 * Matches Stream Android SDK token handling patterns.
 * Uses same base URL and token expiry defaults as Android SDK.
 */

#include "stream_video_token.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>
#include "cJSON.h"

static const char *TAG = "stream_token";

const char* stream_video_get_base_url(stream_video_environment_t env)
{
    // Matches Android demo app: "pronto.getstream.io"
    (void)env; // All environments use same base URL
    return STREAM_VIDEO_DEFAULT_BASE_URL;
}

uint32_t stream_video_get_default_token_expiry(void)
{
    return STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS;
}

bool stream_video_validate_token_format(const char *token)
{
    if (!token) {
        return false;
    }

    // Basic JWT format check: should have 3 parts separated by dots
    // Format: header.payload.signature
    int dot_count = 0;
    for (const char *p = token; *p; p++) {
        if (*p == '.') {
            dot_count++;
        }
    }

    return dot_count == 2;
}

bool stream_video_is_token_expired(const char *token)
{
    if (!token || !stream_video_validate_token_format(token)) {
        return true; // Invalid token considered expired
    }

    // Extract payload (second part of JWT)
    const char *first_dot = strchr(token, '.');
    if (!first_dot) {
        return true;
    }

    const char *second_dot = strchr(first_dot + 1, '.');
    if (!second_dot) {
        return true;
    }

    // Decode base64 payload (simplified - full implementation would use base64 decoder)
    // For now, we'll do a basic check - full validation should be server-side
    // size_t payload_len = second_dot - first_dot - 1;  // Unused for now
    
    // Note: Full JWT decoding would require base64 decoding and JSON parsing
    // This is a placeholder - in production, token expiry should be checked server-side
    // or the expiry time should be provided separately
    
    ESP_LOGD(TAG, "Token expiry check - full validation should be server-side");
    return false; // Assume valid if format is correct (server should validate)
}

