/**
 * @file sfu_client.c
 * @brief SFU WebSocket client for media signaling
 * 
 * Handles connection to Stream's SFU server for WebRTC media signaling.
 * SFU WebSocket URL is obtained from coordinator's joinCall response.
 */

#include "stream_video_sfu.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "sfu.pb.h"
#include "sfu_signal.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "esp_peer.h"
#include "esp_peer_default.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "stream_sfu";

// Event bits for SFU WebSocket events
#define SFU_CONNECTED_BIT    BIT0
#define SFU_DISCONNECTED_BIT BIT1
#define SFU_ERROR_BIT        BIT2

/**
 * @brief SFU client structure
 */
struct stream_sfu_client {
    esp_websocket_client_handle_t ws_client;
    stream_sfu_config_t config;
    stream_sfu_state_t state;
    EventGroupHandle_t event_group;
    TaskHandle_t health_task;
    TickType_t last_event_tick;
    char join_token[2048];
    char join_session_id[128];
    char http_url[256];
    char ws_headers[4096];
    esp_peer_handle_t peer;
    TaskHandle_t peer_task;
    esp_peer_ice_server_cfg_t *ice_servers;
    size_t ice_server_count;
};

static bool encode_string_cb(pb_ostream_t *stream,
                             const pb_field_t *field,
                             void * const *arg)
{
    const char *value = (const char *)(*arg);
    if (!value) {
        return true;
    }
    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }
    return pb_encode_string(stream, (const pb_byte_t *)value, strlen(value));
}

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
} sfu_string_buffer_t;

static bool decode_string_cb(pb_istream_t *stream,
                             const pb_field_t *field,
                             void **arg)
{
    sfu_string_buffer_t *dest = (sfu_string_buffer_t *)(*arg);
    if (!dest || !dest->buffer || dest->capacity == 0) {
        return false;
    }
    size_t len = stream->bytes_left;
    size_t to_copy = len < (dest->capacity - 1) ? len : (dest->capacity - 1);
    if (!pb_read(stream, (pb_byte_t *)dest->buffer, to_copy)) {
        return false;
    }
    dest->length = to_copy;
    dest->buffer[dest->length] = '\0';
    if (len > to_copy) {
        pb_read(stream, NULL, len - to_copy);
    }
    return true;
}

static void sfu_send_health_check(stream_sfu_client_handle_t client)
{
    if (!client || !client->ws_client) {
        return;
    }
    if (!esp_websocket_client_is_connected(client->ws_client)) {
        return;
    }

    uint8_t buffer[16];
    stream_video_sfu_SfuRequest request = stream_video_sfu_SfuRequest_init_zero;
    stream_video_sfu_HealthCheckRequest hc = stream_video_sfu_HealthCheckRequest_init_zero;
    request.which_request_payload = stream_video_sfu_SfuRequest_health_check_request_tag;
    request.request_payload.health_check_request = hc;

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, stream_video_sfu_SfuRequest_fields, &request)) {
        ESP_LOGW(TAG, "Failed to encode SFU health check");
        return;
    }

    int len = esp_websocket_client_send_bin(
        client->ws_client,
        (const char *)buffer,
        (int)stream.bytes_written,
        portMAX_DELAY);
    if (len < 0) {
        ESP_LOGW(TAG, "Failed to send SFU health check");
    }
}

static void sfu_health_monitor_task(void *pvParameters)
{
    stream_sfu_client_handle_t client = (stream_sfu_client_handle_t)pvParameters;
    const TickType_t check_interval = pdMS_TO_TICKS(5000);
    const TickType_t no_event_threshold = pdMS_TO_TICKS(15000);

    while (client) {
        vTaskDelay(check_interval);

        if (client->state == STREAM_SFU_STATE_CONNECTED) {
            sfu_send_health_check(client);

            TickType_t now = xTaskGetTickCount();
            if ((now - client->last_event_tick) > no_event_threshold) {
                if (!esp_websocket_client_is_connected(client->ws_client)) {
                    continue;
                }
                ESP_LOGW(TAG, "No SFU events for 15s, reconnecting...");
                esp_websocket_client_stop(client->ws_client);
                esp_websocket_client_start(client->ws_client);
                client->last_event_tick = xTaskGetTickCount();
            }
        }
    }

    vTaskDelete(NULL);
}

static stream_video_error_t sfu_send_signal_request(
    stream_sfu_client_handle_t client,
    const char *path,
    const uint8_t *payload,
    size_t payload_len)
{
    if (!client || !client->http_url[0] || !path || !payload || payload_len == 0) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    char url[512];
    const char *sep = (client->http_url[strlen(client->http_url) - 1] == '/') ? "" : "/";
    int url_len = snprintf(url, sizeof(url), "%s%s%s", client->http_url, sep, path);
    if (url_len <= 0 || url_len >= (int)sizeof(url)) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
    };
    esp_http_client_handle_t http = esp_http_client_init(&config);
    if (!http) {
        return STREAM_VIDEO_ERR_FAIL;
    }

    esp_http_client_set_method(http, HTTP_METHOD_POST);
    esp_http_client_set_header(http, "Authorization", client->join_token);
    esp_http_client_set_header(http, "stream-auth-type", "jwt");
    esp_http_client_set_header(http, "Content-Type", "application/protobuf");
    esp_http_client_set_header(http, "Accept", "application/protobuf");
    esp_http_client_set_post_field(http, (const char *)payload, payload_len);

    ESP_LOGI(TAG, "SFU HTTP request: %s (%u bytes)", path, (unsigned)payload_len);
    esp_err_t err = esp_http_client_perform(http);
    int status_code = esp_http_client_get_status_code(http);
    esp_http_client_cleanup(http);

    if (err != ESP_OK || status_code < 200 || status_code >= 300) {
        ESP_LOGE(TAG, "SFU signal request failed: %s (HTTP %d)",
                 esp_err_to_name(err), status_code);
        return STREAM_VIDEO_ERR_NETWORK;
    }

    ESP_LOGI(TAG, "SFU signal request OK: %s (HTTP %d)", path, status_code);
    return STREAM_VIDEO_ERR_OK;
}

static stream_video_error_t sfu_send_answer_http(
    stream_sfu_client_handle_t client,
    stream_video_sfu_PeerType peer_type,
    const char *sdp)
{
    if (!client || !sdp || !client->join_session_id[0]) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }
    uint8_t buffer[2048];
    stream_video_sfu_signal_SendAnswerRequest request =
        stream_video_sfu_signal_SendAnswerRequest_init_zero;

    request.peer_type = peer_type;
    request.sdp.funcs.encode = encode_string_cb;
    request.sdp.arg = (void *)sdp;
    request.session_id.funcs.encode = encode_string_cb;
    request.session_id.arg = (void *)client->join_session_id;

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, stream_video_sfu_signal_SendAnswerRequest_fields, &request)) {
        ESP_LOGE(TAG, "Failed to encode SendAnswerRequest");
        return STREAM_VIDEO_ERR_FAIL;
    }

    return sfu_send_signal_request(
        client,
        "stream.video.sfu.signal.SignalServer/SendAnswer",
        buffer,
        stream.bytes_written);
}

static stream_video_error_t sfu_send_ice_trickle_http(
    stream_sfu_client_handle_t client,
    stream_video_sfu_PeerType peer_type,
    const char *candidate)
{
    if (!client || !candidate || !client->join_session_id[0]) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }
    uint8_t buffer[2048];
    stream_video_sfu_signal_ICETrickle request =
        stream_video_sfu_signal_ICETrickle_init_zero;

    request.peer_type = peer_type;
    request.ice_candidate.funcs.encode = encode_string_cb;
    request.ice_candidate.arg = (void *)candidate;
    request.session_id.funcs.encode = encode_string_cb;
    request.session_id.arg = (void *)client->join_session_id;

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, stream_video_sfu_signal_ICETrickle_fields, &request)) {
        ESP_LOGE(TAG, "Failed to encode ICETrickle");
        return STREAM_VIDEO_ERR_FAIL;
    }

    return sfu_send_signal_request(
        client,
        "stream.video.sfu.signal.SignalServer/IceTrickle",
        buffer,
        stream.bytes_written);
}

static int peer_state_handler(esp_peer_state_t state, void *ctx)
{
    (void)state;
    (void)ctx;
    return 0;
}

static int peer_msg_handler(esp_peer_msg_t *info, void *ctx)
{
    stream_sfu_client_handle_t client = (stream_sfu_client_handle_t)ctx;
    if (!client || !info || !info->data) {
        return 0;
    }

    if (info->type == ESP_PEER_MSG_TYPE_SDP) {
        ESP_LOGI(TAG, "Peer generated SDP answer (%d bytes)", info->size);
        sfu_send_answer_http(client, stream_video_sfu_PeerType_PEER_TYPE_SUBSCRIBER, (const char *)info->data);
    } else if (info->type == ESP_PEER_MSG_TYPE_CANDIDATE) {
        ESP_LOGI(TAG, "Peer generated ICE candidate (%d bytes)", info->size);
        sfu_send_ice_trickle_http(client, stream_video_sfu_PeerType_PEER_TYPE_SUBSCRIBER, (const char *)info->data);
    }

    return 0;
}

static void peer_loop_task(void *arg)
{
    stream_sfu_client_handle_t client = (stream_sfu_client_handle_t)arg;
    while (client && client->peer) {
        esp_peer_main_loop(client->peer);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    vTaskDelete(NULL);
}

static stream_video_error_t sfu_peer_ensure(stream_sfu_client_handle_t client)
{
    if (!client || client->peer) {
        return STREAM_VIDEO_ERR_OK;
    }

    ESP_LOGI(TAG, "Creating peer connection (subscriber)");
    esp_peer_default_cfg_t default_cfg = {
        .agent_recv_timeout = 100,
    };
    esp_peer_cfg_t cfg = {
        .server_lists = client->ice_servers,
        .server_num = (uint8_t)client->ice_server_count,
        .role = ESP_PEER_ROLE_CONTROLLED,
        .ice_trans_policy = ESP_PEER_ICE_TRANS_POLICY_ALL,
        .audio_dir = ESP_PEER_MEDIA_DIR_RECV_ONLY,
        .video_dir = ESP_PEER_MEDIA_DIR_RECV_ONLY,
        .audio_info = {
            .codec = ESP_PEER_AUDIO_CODEC_OPUS,
            .sample_rate = 48000,
            .channel = 2,
        },
        .video_info = {
            .codec = ESP_PEER_VIDEO_CODEC_H264,
            .width = 1280,
            .height = 720,
            .fps = 30,
        },
        .enable_data_channel = true,
        .manual_ch_create = false,
        .extra_cfg = &default_cfg,
        .extra_size = sizeof(default_cfg),
        .on_state = peer_state_handler,
        .on_msg = peer_msg_handler,
        .ctx = client,
    };

    if (esp_peer_open(&cfg, esp_peer_get_default_impl(), &client->peer) != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to create peer connection");
        client->peer = NULL;
        return STREAM_VIDEO_ERR_FAIL;
    }

    if (esp_peer_new_connection(client->peer) != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to start peer connection");
        esp_peer_close(client->peer);
        client->peer = NULL;
        return STREAM_VIDEO_ERR_FAIL;
    }

    ESP_LOGI(TAG, "Peer connection started");
    xTaskCreate(peer_loop_task, "sfu_peer", 8192, client, 5, &client->peer_task);
    return STREAM_VIDEO_ERR_OK;
}

static stream_video_error_t sfu_send_join_request(stream_sfu_client_handle_t client)
{
    if (!client || !client->ws_client) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }
    if (!client->join_token[0] || !client->join_session_id[0]) {
        return STREAM_VIDEO_ERR_INVALID_STATE;
    }
    if (!esp_websocket_client_is_connected(client->ws_client)) {
        return STREAM_VIDEO_ERR_INVALID_STATE;
    }

    stream_video_sfu_SfuRequest request = stream_video_sfu_SfuRequest_init_zero;
    stream_video_sfu_JoinRequest join_req = stream_video_sfu_JoinRequest_init_zero;
    request.which_request_payload = stream_video_sfu_SfuRequest_join_request_tag;

    join_req.token.funcs.encode = encode_string_cb;
    join_req.token.arg = (void *)client->join_token;
    join_req.session_id.funcs.encode = encode_string_cb;
    join_req.session_id.arg = (void *)client->join_session_id;
    request.request_payload.join_request = join_req;

    ESP_LOGI(TAG, "SFU join request: token_len=%u session_id=%s",
             (unsigned)strlen(client->join_token),
             client->join_session_id);

    size_t buffer_size = 512;
    uint8_t *buffer = NULL;
    pb_ostream_t stream;
    bool encoded = false;

    for (int attempt = 0; attempt < 3; ++attempt) {
        buffer = (uint8_t *)malloc(buffer_size);
        if (!buffer) {
            ESP_LOGE(TAG, "Failed to allocate join buffer");
            return STREAM_VIDEO_ERR_NO_MEM;
        }

        stream = pb_ostream_from_buffer(buffer, buffer_size);
        encoded = pb_encode(&stream, stream_video_sfu_SfuRequest_fields, &request);
        if (encoded) {
            break;
        }

        ESP_LOGD(TAG, "Join request encode failed: %s", PB_GET_ERROR(&stream));
        free(buffer);
        buffer = NULL;
        buffer_size *= 2;
    }

    if (!encoded) {
        ESP_LOGE(TAG, "Failed to encode SFU join request");
        return STREAM_VIDEO_ERR_FAIL;
    }

    int len = esp_websocket_client_send_bin(
        client->ws_client,
        (const char *)buffer,
        (int)stream.bytes_written,
        portMAX_DELAY);
    free(buffer);
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to send SFU join request");
        return STREAM_VIDEO_ERR_NETWORK;
    }

    return STREAM_VIDEO_ERR_OK;
}

/**
 * @brief SFU WebSocket event handler
 */
static void sfu_websocket_event_handler(void *handler_args, esp_event_base_t base, 
                                        int32_t event_id, void *event_data)
{
    stream_sfu_client_handle_t client = (stream_sfu_client_handle_t)handler_args;
    esp_websocket_event_id_t ws_event_id = (esp_websocket_event_id_t)event_id;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (ws_event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "SFU WebSocket connected");
            client->state = STREAM_SFU_STATE_CONNECTED;
            client->last_event_tick = xTaskGetTickCount();
            xEventGroupSetBits(client->event_group, SFU_CONNECTED_BIT);
            
            // Notify user
            if (client->config.event_cb) {
                stream_sfu_event_t event = {
                    .type = STREAM_SFU_EVENT_CONNECTED,
                    .data = NULL,
                    .data_len = 0
                };
                client->config.event_cb(client, &event, client->config.user_data);
            }

            if (client->join_token[0] && client->join_session_id[0]) {
                stream_video_error_t err = sfu_send_join_request(client);
                if (err != STREAM_VIDEO_ERR_OK) {
                    ESP_LOGW(TAG, "Failed to send SFU join request: %d", err);
                } else {
                    ESP_LOGI(TAG, "✓ SFU join request sent");
                }
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "SFU WebSocket disconnected");
            client->state = STREAM_SFU_STATE_DISCONNECTED;
            xEventGroupSetBits(client->event_group, SFU_DISCONNECTED_BIT);
            
            // Notify user
            if (client->config.event_cb) {
                stream_sfu_event_t event = {
                    .type = STREAM_SFU_EVENT_DISCONNECTED,
                    .data = NULL,
                    .data_len = 0
                };
                client->config.event_cb(client, &event, client->config.user_data);
            }
            break;

        case WEBSOCKET_EVENT_DATA:
            ESP_LOGD(TAG, "SFU received data: opcode=%d, len=%d", data->op_code, data->data_len);
            client->last_event_tick = xTaskGetTickCount();

            if (data->op_code == 0x2) {
                if (data->data_len > 0) {
                    const uint8_t *raw = (const uint8_t *)data->data_ptr;
                    uint8_t key = raw[0];
                    ESP_LOGI(TAG, "SFU first tag byte=0x%02x (field=%u wire=%u)",
                             key, (unsigned)(key >> 3), (unsigned)(key & 0x7));
                    char hex[64] = {0};
                    size_t dump_len = data->data_len < 8 ? (size_t)data->data_len : 8;
                    size_t pos = 0;
                    for (size_t i = 0; i < dump_len; ++i) {
                        pos += snprintf(hex + pos, sizeof(hex) - pos, "%02x ", raw[i]);
                    }
                    ESP_LOGI(TAG, "SFU first bytes: %s", hex);
                }
                char sdp_buf[2048] = {0};
                char ice_buf[1024] = {0};
                char err_buf[128] = {0};
                sfu_string_buffer_t sdp_ctx = { .buffer = sdp_buf, .capacity = sizeof(sdp_buf) };
                sfu_string_buffer_t ice_ctx = { .buffer = ice_buf, .capacity = sizeof(ice_buf) };
                sfu_string_buffer_t err_ctx = { .buffer = err_buf, .capacity = sizeof(err_buf) };

                stream_video_sfu_SfuEvent event = stream_video_sfu_SfuEvent_init_zero;
                event.event_payload.subscriber_offer.sdp.funcs.decode = decode_string_cb;
                event.event_payload.subscriber_offer.sdp.arg = &sdp_ctx;
                event.event_payload.ice_trickle.ice_candidate.funcs.decode = decode_string_cb;
                event.event_payload.ice_trickle.ice_candidate.arg = &ice_ctx;
                event.event_payload.error.error.message.funcs.decode = decode_string_cb;
                event.event_payload.error.error.message.arg = &err_ctx;

                pb_istream_t stream = pb_istream_from_buffer(
                    (const uint8_t *)data->data_ptr,
                    (size_t)data->data_len);
                if (pb_decode(&stream, stream_video_sfu_SfuEvent_fields, &event)) {
                    ESP_LOGI(TAG, "SFU event decoded: which=%d (len=%d)",
                             (int)event.which_event_payload, data->data_len);
                    if (event.which_event_payload == 0) {
                        ESP_LOGW(TAG, "SFU event has no payload tag (len=%d)", data->data_len);
                    }
                    switch (event.which_event_payload) {
                        case stream_video_sfu_SfuEvent_health_check_response_tag:
                            ESP_LOGD(TAG, "SFU health check ack");
                            break;
                        case stream_video_sfu_SfuEvent_subscriber_offer_tag:
                            ESP_LOGI(TAG, "SFU subscriber offer received");
                            ESP_LOGI(TAG, "Subscriber offer SDP bytes: %u", (unsigned)sdp_ctx.length);
                            if (sdp_buf[0]) {
                                sfu_peer_ensure(client);
                                if (client->peer) {
                                    esp_peer_msg_t msg = {
                                        .type = ESP_PEER_MSG_TYPE_SDP,
                                        .data = (uint8_t *)sdp_buf,
                                        .size = (int)strlen(sdp_buf),
                                    };
                                    esp_peer_send_msg(client->peer, &msg);
                                }
                            }
                            break;
                        case stream_video_sfu_SfuEvent_publisher_answer_tag:
                            ESP_LOGI(TAG, "SFU publisher answer received");
                            break;
                        case stream_video_sfu_SfuEvent_ice_trickle_tag:
                            ESP_LOGI(TAG, "SFU ICE trickle received");
                            ESP_LOGI(TAG, "ICE candidate bytes: %u", (unsigned)ice_ctx.length);
                            if (ice_buf[0] && client->peer) {
                                esp_peer_msg_t msg = {
                                    .type = ESP_PEER_MSG_TYPE_CANDIDATE,
                                    .data = (uint8_t *)ice_buf,
                                    .size = (int)strlen(ice_buf),
                                };
                                esp_peer_send_msg(client->peer, &msg);
                            }
                            break;
                        case stream_video_sfu_SfuEvent_join_response_tag:
                            ESP_LOGI(TAG, "SFU join response received");
                            break;
                        case stream_video_sfu_SfuEvent_error_tag:
                            ESP_LOGW(TAG, "SFU error received: code=%d retry=%d msg=%s",
                                     (int)event.event_payload.error.error.code,
                                     event.event_payload.error.error.should_retry,
                                     err_buf[0] ? err_buf : "(empty)");
                            break;
                        default:
                            ESP_LOGW(TAG, "Unhandled SFU event payload: %d", (int)event.which_event_payload);
                            break;
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to decode SFU event: %s (len=%d)",
                             PB_GET_ERROR(&stream), data->data_len);
                }
            }
            
            // Notify user of received message
            if (client->config.event_cb) {
                stream_sfu_event_t event = {
                    .type = STREAM_SFU_EVENT_MESSAGE_RECEIVED,
                    .data = data->data_ptr,
                    .data_len = data->data_len
                };
                client->config.event_cb(client, &event, client->config.user_data);
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "SFU WebSocket error");
            client->state = STREAM_SFU_STATE_ERROR;
            xEventGroupSetBits(client->event_group, SFU_ERROR_BIT);
            
            // Notify user
            if (client->config.event_cb) {
                stream_sfu_event_t event = {
                    .type = STREAM_SFU_EVENT_ERROR,
                    .data = NULL,
                    .data_len = 0
                };
                client->config.event_cb(client, &event, client->config.user_data);
            }
            break;

        default:
            break;
    }
}

stream_video_error_t stream_sfu_client_create(
    const stream_sfu_config_t *config,
    stream_sfu_client_handle_t *client_out)
{
    if (!config || !client_out || !config->ws_endpoint || !config->token) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    stream_sfu_client_handle_t client = (stream_sfu_client_handle_t)calloc(1, sizeof(struct stream_sfu_client));
    if (!client) {
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    // Copy configuration
    memcpy(&client->config, config, sizeof(stream_sfu_config_t));
    client->state = STREAM_SFU_STATE_DISCONNECTED;

    // Create event group
    client->event_group = xEventGroupCreate();
    if (!client->event_group) {
        free(client);
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    int header_len = snprintf(
        client->ws_headers,
        sizeof(client->ws_headers),
        "X-Stream-Client: stream-video-esp32\r\n"
        "stream-auth-type: jwt\r\n"
        "Authorization: %s\r\n",
        config->token);
    if (header_len <= 0 || header_len >= (int)sizeof(client->ws_headers)) {
        vEventGroupDelete(client->event_group);
        free(client);
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    // Configure WebSocket client
    esp_websocket_client_config_t ws_cfg = {
        .uri = config->ws_endpoint,
        .headers = client->ws_headers,
        .reconnect_timeout_ms = 0, // We handle reconnection ourselves if needed
        .task_stack = 8192,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    client->ws_client = esp_websocket_client_init(&ws_cfg);
    if (!client->ws_client) {
        vEventGroupDelete(client->event_group);
        free(client);
        return STREAM_VIDEO_ERR_FAIL;
    }

    // Register event handler
    esp_websocket_register_events(client->ws_client, WEBSOCKET_EVENT_ANY,
                                   sfu_websocket_event_handler, client);

    client->health_task = NULL;
    client->last_event_tick = xTaskGetTickCount();
    client->join_token[0] = '\0';
    client->join_session_id[0] = '\0';
    client->http_url[0] = '\0';
    client->ws_headers[0] = '\0';
    client->peer = NULL;
    client->peer_task = NULL;
    client->ice_servers = NULL;
    client->ice_server_count = 0;
    xTaskCreate(sfu_health_monitor_task, "sfu_health", 4096, client, 5, &client->health_task);

    *client_out = client;
    return STREAM_VIDEO_ERR_OK;
}

void stream_sfu_client_destroy(stream_sfu_client_handle_t client)
{
    if (!client) {
        return;
    }

    // Disconnect if connected
    if (client->ws_client) {
        esp_websocket_client_stop(client->ws_client);
        esp_websocket_client_destroy(client->ws_client);
    }

    // Clean up event group
    if (client->event_group) {
        vEventGroupDelete(client->event_group);
    }

    if (client->health_task) {
        vTaskDelete(client->health_task);
    }

    if (client->peer) {
        esp_peer_close(client->peer);
        client->peer = NULL;
    }
    if (client->peer_task) {
        vTaskDelete(client->peer_task);
    }
    if (client->ice_servers) {
        for (size_t i = 0; i < client->ice_server_count; ++i) {
            free(client->ice_servers[i].stun_url);
            free(client->ice_servers[i].user);
            free(client->ice_servers[i].psw);
        }
        free(client->ice_servers);
        client->ice_servers = NULL;
        client->ice_server_count = 0;
    }

    free(client);
}

stream_video_error_t stream_sfu_client_connect(stream_sfu_client_handle_t client)
{
    if (!client || !client->ws_client) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    if (client->state == STREAM_SFU_STATE_CONNECTED) {
        ESP_LOGW(TAG, "Already connected to SFU");
        return STREAM_VIDEO_ERR_INVALID_STATE;
    }

    client->state = STREAM_SFU_STATE_CONNECTING;

    esp_err_t err = esp_websocket_client_start(client->ws_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start SFU WebSocket client: %s", esp_err_to_name(err));
        client->state = STREAM_SFU_STATE_ERROR;
        return STREAM_VIDEO_ERR_NETWORK;
    }

    return STREAM_VIDEO_ERR_OK;
}

void stream_sfu_client_disconnect(stream_sfu_client_handle_t client)
{
    if (!client || !client->ws_client) {
        return;
    }

    esp_websocket_client_stop(client->ws_client);
    client->state = STREAM_SFU_STATE_DISCONNECTED;
}

stream_video_error_t stream_sfu_client_send(
    stream_sfu_client_handle_t client,
    const uint8_t *message,
    size_t message_len)
{
    if (!client || !message || message_len == 0) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    if (client->state != STREAM_SFU_STATE_CONNECTED) {
        return STREAM_VIDEO_ERR_INVALID_STATE;
    }

    int len = esp_websocket_client_send_text(
        client->ws_client,
        (const char *)message,
        (int)message_len,
        portMAX_DELAY);
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to send SFU message");
        return STREAM_VIDEO_ERR_NETWORK;
    }

    return STREAM_VIDEO_ERR_OK;
}

stream_video_error_t stream_sfu_client_send_join_request(
    stream_sfu_client_handle_t client,
    const char *token,
    const char *session_id)
{
    if (!client || !token || !session_id) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    strncpy(client->join_token, token, sizeof(client->join_token) - 1);
    strncpy(client->join_session_id, session_id, sizeof(client->join_session_id) - 1);

    if (client->state == STREAM_SFU_STATE_CONNECTED) {
        return sfu_send_join_request(client);
    }

    return STREAM_VIDEO_ERR_OK;
}

stream_video_error_t stream_sfu_client_set_join_info(
    stream_sfu_client_handle_t client,
    const char *token,
    const char *session_id)
{
    return stream_sfu_client_send_join_request(client, token, session_id);
}

stream_video_error_t stream_sfu_client_set_http_url(
    stream_sfu_client_handle_t client,
    const char *http_url)
{
    if (!client || !http_url) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }
    strncpy(client->http_url, http_url, sizeof(client->http_url) - 1);
    return STREAM_VIDEO_ERR_OK;
}

stream_video_error_t stream_sfu_client_set_ice_servers(
    stream_sfu_client_handle_t client,
    const esp_peer_ice_server_cfg_t *servers,
    size_t server_count)
{
    if (!client || !servers || server_count == 0) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    client->ice_servers = (esp_peer_ice_server_cfg_t *)calloc(server_count, sizeof(esp_peer_ice_server_cfg_t));
    if (!client->ice_servers) {
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    for (size_t i = 0; i < server_count; ++i) {
        client->ice_servers[i].stun_url = strdup(servers[i].stun_url);
        client->ice_servers[i].user = strdup(servers[i].user);
        client->ice_servers[i].psw = strdup(servers[i].psw);
    }
    client->ice_server_count = server_count;
    return STREAM_VIDEO_ERR_OK;
}

stream_sfu_state_t stream_sfu_client_get_state(stream_sfu_client_handle_t client)
{
    if (!client) {
        return STREAM_SFU_STATE_DISCONNECTED;
    }
    return client->state;
}