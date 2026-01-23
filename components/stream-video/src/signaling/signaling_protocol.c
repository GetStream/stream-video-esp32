/**
 * @file signaling_protocol.c
 * @brief Stream-specific signaling protocol implementation
 * 
 * Implements Stream's signaling protocol messages and message formatting.
 * Handles message construction, parsing, and protocol-specific logic.
 */

#include "stream_video_signaling.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"

// Forward declaration - will be in header when needed
stream_video_error_t stream_signaling_create_connect_message(
    const char *user_id,
    const char *token,
    const char *session_id,
    uint8_t **message_out,
    size_t *message_len_out);

stream_video_error_t stream_signaling_create_join_room_message(
    const char *room_id,
    const char *user_id,
    uint8_t **message_out,
    size_t *message_len_out);

stream_video_error_t stream_signaling_create_publish_track_message(
    const char *track_id,
    const char *kind,
    const char *track_type,
    uint8_t **message_out,
    size_t *message_len_out);

stream_video_error_t stream_signaling_parse_message(
    const uint8_t *message,
    size_t message_len,
    int *msg_type_out,
    cJSON **json_out);

static const char *TAG = "stream_signaling_protocol";

// Message types for Stream signaling protocol
typedef enum {
    STREAM_MSG_TYPE_CONNECT = 1,
    STREAM_MSG_TYPE_DISCONNECT,
    STREAM_MSG_TYPE_JOIN_ROOM,
    STREAM_MSG_TYPE_LEAVE_ROOM,
    STREAM_MSG_TYPE_PUBLISH_TRACK,
    STREAM_MSG_TYPE_UNPUBLISH_TRACK,
    STREAM_MSG_TYPE_SUBSCRIBE_TRACK,
    STREAM_MSG_TYPE_UNSUBSCRIBE_TRACK,
} stream_msg_type_t;

/**
 * @brief Create a connect message
 * 
 * @param user_id User ID
 * @param token Authentication token
 * @param session_id Session ID (optional, can be NULL)
 * @param message_out Output message buffer (caller must free)
 * @param message_len_out Output message length
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_signaling_create_connect_message(
    const char *user_id,
    const char *token,
    const char *session_id,
    uint8_t **message_out,
    size_t *message_len_out)
{
    if (!user_id || !token || !message_out || !message_len_out) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    (void)session_id;
    cJSON_AddStringToObject(json, "type", "authenticate");
    cJSON_AddStringToObject(json, "token", token);

    cJSON *user_details = cJSON_CreateObject();
    if (!user_details) {
        cJSON_Delete(json);
        return STREAM_VIDEO_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(user_details, "id", user_id);
    cJSON_AddStringToObject(user_details, "name", user_id);
    cJSON_AddItemToObject(json, "user_details", user_details);

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string) {
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    *message_out = (uint8_t *)json_string;
    *message_len_out = strlen(json_string);

    return STREAM_VIDEO_ERR_OK;
}

/**
 * @brief Create a join room message
 * 
 * @param room_id Room ID
 * @param user_id User ID
 * @param message_out Output message buffer (caller must free)
 * @param message_len_out Output message length
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_signaling_create_join_room_message(
    const char *room_id,
    const char *user_id,
    uint8_t **message_out,
    size_t *message_len_out)
{
    if (!room_id || !user_id || !message_out || !message_len_out) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(json, "type", STREAM_MSG_TYPE_JOIN_ROOM);
    cJSON_AddStringToObject(json, "room_id", room_id);
    cJSON_AddStringToObject(json, "user_id", user_id);

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string) {
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    *message_out = (uint8_t *)json_string;
    *message_len_out = strlen(json_string);

    return STREAM_VIDEO_ERR_OK;
}

/**
 * @brief Create a publish track message
 * 
 * @param track_id Track ID
 * @param kind Track kind ("audio" or "video")
 * @param track_type Track type (optional, can be NULL)
 * @param message_out Output message buffer (caller must free)
 * @param message_len_out Output message length
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_signaling_create_publish_track_message(
    const char *track_id,
    const char *kind,
    const char *track_type,
    uint8_t **message_out,
    size_t *message_len_out)
{
    if (!track_id || !kind || !message_out || !message_len_out) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(json, "type", STREAM_MSG_TYPE_PUBLISH_TRACK);
    cJSON_AddStringToObject(json, "track_id", track_id);
    cJSON_AddStringToObject(json, "kind", kind);
    if (track_type) {
        cJSON_AddStringToObject(json, "track_type", track_type);
    }

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string) {
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    *message_out = (uint8_t *)json_string;
    *message_len_out = strlen(json_string);

    return STREAM_VIDEO_ERR_OK;
}

/**
 * @brief Parse incoming signaling message
 * 
 * @param message Message data
 * @param message_len Message length
 * @param msg_type_out Output message type
 * @param json_out Output JSON object (caller must free with cJSON_Delete)
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_signaling_parse_message(
    const uint8_t *message,
    size_t message_len,
    int *msg_type_out,
    cJSON **json_out)
{
    if (!message || message_len == 0 || !msg_type_out || !json_out) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    // Parse JSON
    cJSON *json = cJSON_ParseWithLength((const char *)message, message_len);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON message");
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    // Extract message type (number or string)
    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    if (!type_item) {
        cJSON *event_type = cJSON_GetObjectItem(json, "event_type");
        if (event_type && cJSON_IsString(event_type)) {
            cJSON_AddStringToObject(json, "type", event_type->valuestring);
            type_item = cJSON_GetObjectItem(json, "type");
        }
    }
    if (!type_item) {
        ESP_LOGE(TAG, "Invalid message type");
        cJSON_Delete(json);
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    if (cJSON_IsNumber(type_item)) {
        *msg_type_out = type_item->valueint;
    } else {
        *msg_type_out = 0;
    }
    *json_out = json;

    return STREAM_VIDEO_ERR_OK;
}