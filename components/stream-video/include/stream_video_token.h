/**
 * @file stream_video_token.h
 * @brief Token generation and management utilities
 * 
 * Matches Stream Android SDK token handling patterns.
 */

#ifndef STREAM_VIDEO_TOKEN_H
#define STREAM_VIDEO_TOKEN_H

#include "stream_video.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default token expiry time (7 days) - matches Android demo app
 */
#define STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS (7 * 24 * 60 * 60)  // 7 days

/**
 * @brief Default base URL - matches Android demo app
 */
#define STREAM_VIDEO_DEFAULT_BASE_URL "pronto.getstream.io"

/**
 * @brief Environment types for token generation
 */
typedef enum {
    STREAM_VIDEO_ENV_PRODUCTION = 0,
    STREAM_VIDEO_ENV_STAGING,
    STREAM_VIDEO_ENV_DEVELOPMENT,
} stream_video_environment_t;

/**
 * @brief Token generation request
 * 
 * This structure is used when requesting a token from your backend.
 * The backend should generate the actual JWT token.
 */
typedef struct {
    const char *api_key;              // Stream API key
    const char *user_id;              // User ID
    stream_video_environment_t env;   // Environment (production/staging/development)
    uint32_t expiry_seconds;          // Token expiry in seconds (default: 3600 = 1 hour)
    const char *custom_claims;        // Optional: JSON string with custom claims
} stream_video_token_request_t;

/**
 * @brief Get base URL for given environment
 * 
 * Matches Android SDK pattern - returns the base URL based on environment.
 * 
 * @param env Environment type
 * @return const char* Base URL string
 */
const char* stream_video_get_base_url(stream_video_environment_t env);

/**
 * @brief Get default token expiry time
 * 
 * Returns the default token expiry time used by Stream Android SDK (1 hour).
 * 
 * @return uint32_t Expiry time in seconds
 */
uint32_t stream_video_get_default_token_expiry(void);

/**
 * @brief Validate token format (basic check)
 * 
 * Performs basic validation on a JWT token string.
 * Note: Full JWT validation should be done server-side.
 * 
 * @param token Token string to validate
 * @return true if token format looks valid, false otherwise
 */
bool stream_video_validate_token_format(const char *token);

/**
 * @brief Check if token is expired (if expiry can be extracted)
 * 
 * Attempts to extract expiry from JWT and check if expired.
 * Note: This is a basic check - full validation should be server-side.
 * 
 * @param token JWT token string
 * @return true if token appears expired, false otherwise
 */
bool stream_video_is_token_expired(const char *token);

#ifdef __cplusplus
}
#endif

#endif // STREAM_VIDEO_TOKEN_H

