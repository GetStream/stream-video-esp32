/**
 * @file stream_video_auth.h
 * @brief Authentication and user management
 * 
 * Matches Stream Android SDK demo app authentication flow.
 */

#ifndef STREAM_VIDEO_AUTH_H
#define STREAM_VIDEO_AUTH_H

#include "stream_video.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief User role
 */
typedef enum {
    STREAM_VIDEO_USER_ROLE_ADMIN = 0,
    STREAM_VIDEO_USER_ROLE_USER,
    STREAM_VIDEO_USER_ROLE_GUEST,
} stream_video_user_role_t;

/**
 * @brief User object
 * 
 * Matches Android SDK User structure.
 * At minimum requires id and role.
 */
typedef struct {
    const char *id;                    // User ID
    stream_video_user_role_t role;     // User role (admin/user/guest)
    const char *name;                  // Optional: User name
    const char *image;                 // Optional: User image URL
} stream_video_user_t;

/**
 * @brief Auth data response
 * 
 * Matches Android SDK GetAuthDataResponse.
 */
typedef struct {
    char user_id[128];                 // User ID from response
    char api_key[256];                 // API key from response
    char token[512];                   // JWT token from response
} stream_video_auth_data_t;

/**
 * @brief Auth data request parameters
 */
typedef struct {
    const char *environment;           // Environment name (e.g., "production", "staging")
    const char *user_id;               // Optional: User ID (can be NULL)
    uint32_t exp;                      // Token expiry in seconds (default: 7 days)
} stream_video_auth_request_t;

/**
 * @brief Callback for auth data response
 * 
 * @param auth_data Auth data response
 * @param user_data User-provided context
 * @return stream_video_error_t Error code
 */
typedef stream_video_error_t (*stream_video_auth_callback_t)(
    const stream_video_auth_data_t *auth_data,
    void *user_data
);

/**
 * @brief Request auth data from backend
 * 
 * Makes GET request to: api/auth/create-token
 * Query params: environment, user_id (optional), exp
 * 
 * This should be implemented by the application to call their backend.
 * 
 * @param request Auth request parameters
 * @param callback Callback when auth data is received
 * @param user_data User context for callback
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_video_request_auth_data(
    const stream_video_auth_request_t *request,
    stream_video_auth_callback_t callback,
    void *user_data
);

/**
 * @brief Create user object from auth data
 * 
 * Creates a user object with id and role (at minimum).
 * Matches Android SDK pattern: User(id = authData.userId, role = "admin")
 * 
 * @param auth_data Auth data response
 * @param role User role (default: admin)
 * @param user_out Output user object
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_video_create_user_from_auth(
    const stream_video_auth_data_t *auth_data,
    stream_video_user_role_t role,
    stream_video_user_t *user_out
);

#ifdef __cplusplus
}
#endif

#endif // STREAM_VIDEO_AUTH_H

