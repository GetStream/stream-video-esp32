/**
 * @file app_token.h
 * @brief App-side token request (example implementation)
 *
 * The app is responsible for obtaining auth data. This module implements
 * a request to a token service (e.g. Stream's pronto or your own backend).
 * Include this and app_token.c in your app when you want to fetch tokens
 * from an HTTP endpoint that returns { userId, apiKey, token }.
 */

#ifndef APP_TOKEN_H
#define APP_TOKEN_H

#include "stream_video_auth.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Request auth data from a token service (blocking)
 *
 * Performs GET request to: base_url + "api/auth/create-token?environment=...&exp=...&user_id=..."
 * Expects JSON response: { "userId": "...", "apiKey": "...", "token": "..." }
 *
 * @param base_url     Full base URL including trailing slash (e.g. "https://pronto.getstream.io/")
 * @param environment  Environment name (e.g. "production", "pronto")
 * @param user_id      Optional user ID (can be NULL)
 * @param exp          Token expiry in seconds (e.g. 7 days)
 * @param auth_data_out Filled on success
 * @return stream_video_error_t STREAM_VIDEO_ERR_OK on success
 */
stream_video_error_t app_request_auth_data(
    const char *base_url,
    const char *environment,
    const char *user_id,
    uint32_t exp,
    stream_video_auth_data_t *auth_data_out);

#ifdef __cplusplus
}
#endif

#endif /* APP_TOKEN_H */
