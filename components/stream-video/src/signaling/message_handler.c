/**
 * @file message_handler.c
 * @brief Handle incoming/outgoing signaling messages
 * 
 * Routes incoming messages to appropriate handlers and manages message lifecycle.
 */

#include "stream_video_signaling.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "stream_signaling_msg";

// Forward declaration from signaling_protocol.c
extern stream_video_error_t stream_signaling_parse_message(
    const uint8_t *message,
    size_t message_len,
    int *msg_type_out,
    cJSON **json_out);

/**
 * @brief Handle connect response message
 */
static void handle_connect_response(stream_signaling_client_handle_t client, cJSON *json)
{
    cJSON *success_item = cJSON_GetObjectItem(json, "success");
    if (success_item && cJSON_IsTrue(success_item)) {
        ESP_LOGI(TAG, "Connect successful");
        cJSON *session_id = cJSON_GetObjectItem(json, "session_id");
        if (session_id && cJSON_IsString(session_id)) {
            ESP_LOGI(TAG, "Session ID: %s", session_id->valuestring);
        }
    } else {
        ESP_LOGE(TAG, "Connect failed");
        cJSON *error = cJSON_GetObjectItem(json, "error_message");
        if (error && cJSON_IsString(error)) {
            ESP_LOGE(TAG, "Error: %s", error->valuestring);
        }
    }
}

/**
 * @brief Handle connection ok event
 */
static void handle_connection_ok(stream_signaling_client_handle_t client, cJSON *json)
{
    cJSON *connection_id = cJSON_GetObjectItem(json, "connection_id");
    if (connection_id && cJSON_IsString(connection_id)) {
        stream_signaling_client_set_connection_id(client, connection_id->valuestring);
        ESP_LOGI(TAG, "Connection ID: %s", connection_id->valuestring);
    }
}

/**
 * @brief Handle connection error event
 */
static void handle_connection_error(stream_signaling_client_handle_t client, cJSON *json)
{
    (void)client;
    cJSON *error = cJSON_GetObjectItem(json, "error");
    if (error) {
        cJSON *code = cJSON_GetObjectItem(error, "code");
        cJSON *message = cJSON_GetObjectItem(error, "message");
        if (message && cJSON_IsString(message)) {
            if (code && cJSON_IsNumber(code)) {
                ESP_LOGE(TAG, "Connection error: code=%d, message=%s",
                         code->valueint, message->valuestring);
            } else {
                ESP_LOGE(TAG, "Connection error: %s", message->valuestring);
            }
        }
    }
}

/**
 * @brief Handle participant count updates
 */
static void handle_participant_count_updated(stream_signaling_client_handle_t client, cJSON *json)
{
    (void)client;
    cJSON *participants_by_role = cJSON_GetObjectItem(json, "participants_count_by_role");
    if (participants_by_role && cJSON_IsObject(participants_by_role)) {
        ESP_LOGD(TAG, "Participant counts updated");
    }
}

/**
 * @brief Handle join room response message
 */
static void handle_join_room_response(stream_signaling_client_handle_t client, cJSON *json)
{
    cJSON *success_item = cJSON_GetObjectItem(json, "success");
    if (success_item && cJSON_IsTrue(success_item)) {
        ESP_LOGI(TAG, "Join room successful");
    } else {
        ESP_LOGE(TAG, "Join room failed");
        cJSON *error = cJSON_GetObjectItem(json, "error_message");
        if (error && cJSON_IsString(error)) {
            ESP_LOGE(TAG, "Error: %s", error->valuestring);
        }
    }
}

/**
 * @brief Handle track published event
 */
static void handle_track_published_event(stream_signaling_client_handle_t client, cJSON *json)
{
    cJSON *track_id = cJSON_GetObjectItem(json, "track_id");
    cJSON *kind = cJSON_GetObjectItem(json, "kind");
    cJSON *participant_id = cJSON_GetObjectItem(json, "participant_id");

    if (track_id && cJSON_IsString(track_id) &&
        kind && cJSON_IsString(kind)) {
        ESP_LOGI(TAG, "Track published: id=%s, kind=%s", 
                 track_id->valuestring, kind->valuestring);
    }
}

/**
 * @brief Handle track subscribed event
 */
static void handle_track_subscribed_event(stream_signaling_client_handle_t client, cJSON *json)
{
    cJSON *track_id = cJSON_GetObjectItem(json, "track_id");
    cJSON *kind = cJSON_GetObjectItem(json, "kind");
    cJSON *participant_id = cJSON_GetObjectItem(json, "participant_id");

    if (track_id && cJSON_IsString(track_id) &&
        kind && cJSON_IsString(kind)) {
        ESP_LOGI(TAG, "Track subscribed: id=%s, kind=%s", 
                 track_id->valuestring, kind->valuestring);
    }
}

/**
 * @brief Handle participant joined event
 */
static void handle_participant_joined_event(stream_signaling_client_handle_t client, cJSON *json)
{
    cJSON *participant_id = cJSON_GetObjectItem(json, "participant_id");
    cJSON *user_id = cJSON_GetObjectItem(json, "user_id");

    if (participant_id && cJSON_IsString(participant_id)) {
        ESP_LOGI(TAG, "Participant joined: %s", participant_id->valuestring);
    }
}

/**
 * @brief Handle participant left event
 */
static void handle_participant_left_event(stream_signaling_client_handle_t client, cJSON *json)
{
    cJSON *participant_id = cJSON_GetObjectItem(json, "participant_id");

    if (participant_id && cJSON_IsString(participant_id)) {
        ESP_LOGI(TAG, "Participant left: %s", participant_id->valuestring);
    }
}

/**
 * @brief Handle error response
 */
static void handle_error_response(stream_signaling_client_handle_t client, cJSON *json)
{
    cJSON *code = cJSON_GetObjectItem(json, "code");
    cJSON *message = cJSON_GetObjectItem(json, "message");

    if (code && cJSON_IsNumber(code) && message && cJSON_IsString(message)) {
        ESP_LOGE(TAG, "Error response: code=%d, message=%s", 
                 code->valueint, message->valuestring);
    }
}

/**
 * @brief Process incoming signaling message
 * 
 * @param client Signaling client
 * @param message Message data
 * @param message_len Message length
 */
void stream_signaling_handle_message(
    stream_signaling_client_handle_t client,
    const uint8_t *message,
    size_t message_len)
{
    if (!client || !message || message_len == 0) {
        return;
    }

    int msg_type = 0;
    cJSON *json = NULL;

    stream_video_error_t err = stream_signaling_parse_message(message, message_len, &msg_type, &json);
    if (err != STREAM_VIDEO_ERR_OK || !json) {
        ESP_LOGE(TAG, "Failed to parse message");
        return;
    }

    // Route message based on type
    // Note: Message types from server are typically different from client message types
    // This is a simplified handler - actual implementation should match Stream's protocol
    
    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    if (type_item && cJSON_IsString(type_item)) {
        const char *type_str = type_item->valuestring;
        
        if (strcmp(type_str, "connection.ok") == 0) {
            handle_connection_ok(client, json);
        } else if (strcmp(type_str, "connection.error") == 0) {
            handle_connection_error(client, json);
        } else if (strcmp(type_str, "health.check") == 0) {
            ESP_LOGD(TAG, "Health check received");
        } else if (strcmp(type_str, "call.session_participant_count_updated") == 0) {
            handle_participant_count_updated(client, json);
        } else if (strcmp(type_str, "connect_response") == 0) {
            handle_connect_response(client, json);
        } else if (strcmp(type_str, "join_room_response") == 0) {
            handle_join_room_response(client, json);
        } else if (strcmp(type_str, "track_published") == 0) {
            handle_track_published_event(client, json);
        } else if (strcmp(type_str, "track_subscribed") == 0) {
            handle_track_subscribed_event(client, json);
        } else if (strcmp(type_str, "participant_joined") == 0) {
            handle_participant_joined_event(client, json);
        } else if (strcmp(type_str, "participant_left") == 0 ||
                   strcmp(type_str, "call.session_participant_left") == 0) {
            handle_participant_left_event(client, json);
        } else if (strcmp(type_str, "error") == 0) {
            handle_error_response(client, json);
        } else {
            ESP_LOGW(TAG, "Unknown message type: %s", type_str);
        }
    } else if (type_item && cJSON_IsNumber(type_item)) {
        // Handle numeric message types
        int type_num = type_item->valueint;
        ESP_LOGD(TAG, "Received message type: %d", type_num);
        // Add specific handlers for numeric types if needed
    }

    cJSON_Delete(json);
}