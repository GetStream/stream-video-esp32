/**
 * @file auth_client.c
 * @brief Auth helpers: create user from auth data
 *
 * Token fetching is the app's responsibility. The app obtains auth data
 * (userId, apiKey, token) from its own backend or Stream's token service
 * and passes it to the SDK. This file only provides stream_video_create_user_from_auth.
 */

#include "stream_video_auth.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "stream_auth";

stream_video_error_t stream_video_create_user_from_auth(
    const stream_video_auth_data_t *auth_data,
    stream_video_user_role_t role,
    stream_video_user_t *user_out)
{
    if (!auth_data || !user_out || !auth_data->user_id[0]) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    user_out->id = auth_data->user_id;
    user_out->role = role;
    user_out->name = NULL;
    user_out->image = NULL;

    ESP_LOGD(TAG, "Created user: id=%s, role=%d", user_out->id, user_out->role);

    return STREAM_VIDEO_ERR_OK;
}
