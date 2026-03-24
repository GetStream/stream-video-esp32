/**
 * @file sfu_client.c
 * @brief SFU WebSocket client for media signaling
 * 
 * Handles connection to Stream's SFU server for WebRTC media signaling.
 * SFU WebSocket URL is obtained from coordinator's joinCall response.
 */

#include "stream_video_sfu.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "sfu.pb.h"
#include "sfu_signal.pb.h"
#include "models.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "esp_peer.h"
#include "esp_peer_default.h"
#include "esp_http_client.h"
#include "esp_capture_sink.h"
#include "esp_capture_types.h"
#include "esp_system.h"
#include "esp_timer.h"
#ifdef CONFIG_STREAM_AUDIO_DUMP_OPUS
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

static const char *TAG = "stream_sfu";

#define SFU_HTTP_BUFFER_SIZE 8192

#define SFU_PEER_TASK_STACK_BYTES 16384
#define SFU_PUB_TASK_STACK_BYTES 24576

#define SFU_MAX_PUBLISH_TRACK_IDS 4
#ifdef CONFIG_STREAM_AUDIO_CHANNELS
#define STREAM_SFU_AUDIO_CHANNELS CONFIG_STREAM_AUDIO_CHANNELS
#else
#define STREAM_SFU_AUDIO_CHANNELS 2
#endif
#ifdef CONFIG_STREAM_AUDIO_DUMP_OPUS
#ifndef CONFIG_STREAM_AUDIO_DUMP_SECONDS
#define CONFIG_STREAM_AUDIO_DUMP_SECONDS 30
#endif
#ifndef CONFIG_STREAM_AUDIO_DUMP_MAX_BYTES
#define CONFIG_STREAM_AUDIO_DUMP_MAX_BYTES (256 * 1024)
#endif
static uint8_t *s_opus_dump_buf = NULL;
static size_t s_opus_dump_len = 0;
static size_t s_opus_dump_cap = 0;
#endif

// Event bits for SFU WebSocket events
#define SFU_CONNECTED_BIT    BIT0
#define SFU_DISCONNECTED_BIT BIT1
#define SFU_ERROR_BIT        BIT2

typedef struct {
    char mid[16];
    char track_id[64];
} sfu_track_id_entry_t;

typedef struct {
    stream_video_sfu_models_TrackType track_type;
    int32_t id;
    char codec_name[16];
    uint32_t fps;
    uint32_t width;
    uint32_t height;
} sfu_publish_option_t;

/**
 * @brief SFU client structure
 */
struct stream_sfu_client {
    esp_websocket_client_handle_t ws_client;
    stream_sfu_config_t config;
    stream_sfu_state_t state;
    EventGroupHandle_t event_group;
    TaskHandle_t health_task;
    TaskHandle_t reconnect_task;
    bool should_reconnect;
    uint32_t reconnect_attempts;
    TickType_t last_event_tick;
    char join_token[2048];
    char join_session_id[128];
    char http_url[256];
    char ws_headers[4096];
    esp_peer_handle_t peer;
    TaskHandle_t peer_task;
    esp_peer_handle_t pub_peer;
    TaskHandle_t pub_task;
    TaskHandle_t publish_task;
    esp_capture_sink_handle_t publish_sink;
    bool publish_audio;
    bool publish_video;
    bool publish_running;
    sfu_track_id_entry_t publish_track_ids[SFU_MAX_PUBLISH_TRACK_IDS];
    size_t publish_track_id_count;
    sfu_publish_option_t publish_options[SFU_MAX_PUBLISH_TRACK_IDS];
    size_t publish_option_count;
    esp_peer_ice_server_cfg_t *ice_servers;
    size_t ice_server_count;
    TickType_t ws_connected_tick;
    bool has_joined;
    bool publisher_ready;
    uint32_t reconnect_attempt;
    char http_auth_header[2100];
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

static stream_video_error_t sfu_send_ice_trickle_http(
    stream_sfu_client_handle_t client,
    stream_video_sfu_PeerType peer_type,
    const char *candidate);

static void sfu_send_trickle_from_sdp(stream_sfu_client_handle_t client, const char *sdp)
{
    if (!client || !sdp || !client->join_session_id[0]) {
        return;
    }
    const char *cursor = sdp;
    unsigned sent = 0;
    while (*cursor) {
        const char *line_end = strchr(cursor, '\n');
        size_t line_len = line_end ? (size_t)(line_end - cursor) : strlen(cursor);
        if (line_len > 0 && cursor[line_len - 1] == '\r') {
            line_len--;
        }
        if (line_len > 12 && strncmp(cursor, "a=candidate:", 12) == 0) {
            char candidate[256];
            size_t copy_len = line_len - 2; // skip "a="
            if (copy_len >= sizeof(candidate)) {
                copy_len = sizeof(candidate) - 1;
            }
            memcpy(candidate, cursor + 2, copy_len);
            candidate[copy_len] = '\0';
            sfu_send_ice_trickle_http(
                client,
                stream_video_sfu_PeerType_PEER_TYPE_PUBLISHER_UNSPECIFIED,
                candidate);
            sent++;
        }
        if (!line_end) {
            break;
        }
        cursor = line_end + 1;
    }
    if (sent > 0) {
        ESP_LOGI(TAG, "Sent %u ICE candidates from SDP", sent);
    } else {
        ESP_LOGW(TAG, "No ICE candidates found in SDP for trickle");
    }
}

static bool sfu_extract_candidate_from_json(const char *json, char *out, size_t out_size)
{
    if (!json || !out || out_size == 0) {
        return false;
    }
    const char *key = "\"candidate\"";
    const char *pos = strstr(json, key);
    if (!pos) {
        return false;
    }
    pos = strchr(pos + strlen(key), ':');
    if (!pos) {
        return false;
    }
    pos = strchr(pos, '"');
    if (!pos) {
        return false;
    }
    pos++;
    size_t i = 0;
    while (*pos && *pos != '"' && i + 1 < out_size) {
        if (*pos == '\\' && pos[1]) {
            pos++;
        }
        out[i++] = *pos++;
    }
    out[i] = '\0';
    return i > 0;
}

typedef struct {
    char track_id[64];
    char mid[16];
    stream_video_sfu_models_TrackType track_type;
    bool muted;
    int payload_type;
    uint32_t clock_rate;
    char codec_name[16];
    char encoding_params[16];
    char fmtp[128];
    uint32_t video_width;
    uint32_t video_height;
    uint32_t video_fps;
    uint32_t video_bitrate;
    char video_rid[4];
    int32_t publish_option_id;
} sfu_track_info_t;

typedef struct {
    const sfu_track_info_t *tracks;
    size_t count;
} sfu_track_list_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t bitrate;
    const char *rid;
} sfu_video_layer_info_t;

typedef struct {
    sfu_publish_option_t *options;
    size_t capacity;
    size_t count;
} sfu_publish_options_ctx_t;

static void sfu_copy_token(char *dest, size_t dest_size, const char *start)
{
    if (!dest || dest_size == 0) {
        return;
    }
    size_t len = 0;
    while (start[len] && start[len] != ' ' && start[len] != '\r' && start[len] != '\n') {
        ++len;
    }
    if (len >= dest_size) {
        len = dest_size - 1;
    }
    memcpy(dest, start, len);
    dest[len] = '\0';
}

static const char *sfu_track_type_to_str(stream_video_sfu_models_TrackType type)
{
    switch (type) {
        case stream_video_sfu_models_TrackType_TRACK_TYPE_AUDIO:
            return "TRACK_TYPE_AUDIO";
        case stream_video_sfu_models_TrackType_TRACK_TYPE_VIDEO:
            return "TRACK_TYPE_VIDEO";
        case stream_video_sfu_models_TrackType_TRACK_TYPE_SCREEN_SHARE:
            return "TRACK_TYPE_SCREEN_SHARE";
        case stream_video_sfu_models_TrackType_TRACK_TYPE_SCREEN_SHARE_AUDIO:
            return "TRACK_TYPE_SCREEN_SHARE_AUDIO";
        default:
            return "TRACK_TYPE_UNSPECIFIED";
    }
}

static char *sfu_escape_json_string(const char *input)
{
    if (!input) {
        return NULL;
    }
    size_t len = strlen(input);
    size_t cap = len * 2 + 1;
    char *out = (char *)malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t pos = 0;
    for (size_t i = 0; i < len; ++i) {
        char ch = input[i];
        const char *rep = NULL;
        switch (ch) {
            case '\\': rep = "\\\\"; break;
            case '"': rep = "\\\""; break;
            case '\n': rep = "\\n"; break;
            case '\r': rep = "\\r"; break;
            case '\t': rep = "\\t"; break;
            default: break;
        }
        if (rep) {
            size_t rep_len = strlen(rep);
            if (pos + rep_len + 1 >= cap) {
                cap *= 2;
                char *tmp = (char *)realloc(out, cap);
                if (!tmp) {
                    free(out);
                    return NULL;
                }
                out = tmp;
            }
            memcpy(out + pos, rep, rep_len);
            pos += rep_len;
        } else {
            if (pos + 2 >= cap) {
                cap *= 2;
                char *tmp = (char *)realloc(out, cap);
                if (!tmp) {
                    free(out);
                    return NULL;
                }
                out = tmp;
            }
            out[pos++] = ch;
        }
    }
    out[pos] = '\0';
    return out;
}

typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
} sfu_string_builder_t;

static void sfu_builder_init(sfu_string_builder_t *builder)
{
    builder->buffer = NULL;
    builder->length = 0;
    builder->capacity = 0;
}

static bool sfu_builder_append(sfu_string_builder_t *builder, const char *text)
{
    if (!builder || !text) {
        return false;
    }
    size_t add_len = strlen(text);
    if (add_len == 0) {
        return true;
    }
    size_t needed = builder->length + add_len + 1;
    if (needed > builder->capacity) {
        size_t new_cap = builder->capacity == 0 ? 256 : builder->capacity;
        while (new_cap < needed) {
            new_cap *= 2;
        }
        char *tmp = (char *)realloc(builder->buffer, new_cap);
        if (!tmp) {
            return false;
        }
        builder->buffer = tmp;
        builder->capacity = new_cap;
    }
    memcpy(builder->buffer + builder->length, text, add_len);
    builder->length += add_len;
    builder->buffer[builder->length] = '\0';
    return true;
}

static void sfu_builder_reset(sfu_string_builder_t *builder)
{
    if (!builder) {
        return;
    }
    free(builder->buffer);
    builder->buffer = NULL;
    builder->length = 0;
    builder->capacity = 0;
}

static void sfu_append_filtered_video_section(sfu_string_builder_t *out, const char *section)
{
    if (!out || !section) {
        return;
    }

    int preferred_pt = -1;
    char line[512];
    const char *cursor = section;
    while (*cursor) {
        size_t len = 0;
        while (*cursor && *cursor != '\n' && len + 1 < sizeof(line)) {
            line[len++] = *cursor++;
        }
        if (*cursor == '\n') {
            cursor++;
        }
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        } else {
            line[len] = '\0';
        }

        if (strncmp(line, "a=fmtp:", 7) == 0 && strstr(line, "packetization-mode=1")) {
            int pt = -1;
            if (sscanf(line, "a=fmtp:%d", &pt) == 1) {
                preferred_pt = pt;
                break;
            }
        }
        if (strncmp(line, "a=rtpmap:", 9) == 0 && strstr(line, "H264/")) {
            int pt = -1;
            if (sscanf(line, "a=rtpmap:%d", &pt) == 1 && preferred_pt < 0) {
                preferred_pt = pt;
            }
        }
    }

    cursor = section;
    while (*cursor) {
        size_t len = 0;
        while (*cursor && *cursor != '\n' && len + 1 < sizeof(line)) {
            line[len++] = *cursor++;
        }
        if (*cursor == '\n') {
            cursor++;
        }
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        } else {
            line[len] = '\0';
        }

        if (strncmp(line, "m=video", 7) == 0) {
            if (preferred_pt > 0) {
                char mline[64];
                snprintf(mline, sizeof(mline), "m=video 9 UDP/TLS/RTP/SAVPF %d\r\n", preferred_pt);
                sfu_builder_append(out, mline);
            } else {
                sfu_builder_append(out, line);
                sfu_builder_append(out, "\r\n");
            }
            continue;
        }

        if ((strncmp(line, "a=rtpmap:", 9) == 0) ||
            (strncmp(line, "a=fmtp:", 7) == 0) ||
            (strncmp(line, "a=rtcp-fb:", 10) == 0)) {
            int pt = -1;
            if (sscanf(line, "a=rtpmap:%d", &pt) != 1 &&
                sscanf(line, "a=fmtp:%d", &pt) != 1 &&
                sscanf(line, "a=rtcp-fb:%d", &pt) != 1) {
                continue;
            }
            if (preferred_pt > 0 && pt != preferred_pt) {
                continue;
            }
        }

        sfu_builder_append(out, line);
        sfu_builder_append(out, "\r\n");
    }
}

static char *sfu_filter_sdp(const char *sdp, bool include_audio, size_t *out_len)
{
    if (!sdp) {
        return NULL;
    }

    sfu_string_builder_t session_builder;
    sfu_string_builder_t section_builder;
    sfu_string_builder_t section_out;
    sfu_builder_init(&session_builder);
    sfu_builder_init(&section_builder);
    sfu_builder_init(&section_out);

    char mids[4][16] = {{0}};
    size_t mid_count = 0;
    bool in_section = false;
    bool keep_section = false;
    int current_mid_index = -1;
    bool current_is_video = false;
    bool current_is_audio = false;
    bool audio_has_fmtp_111 = false;
    bool audio_has_rtcpfb_111 = false;
    bool audio_has_red_rtpmap = false;
    bool audio_has_red_fmtp = false;
    char line[512];
    const char *cursor = sdp;
    while (*cursor) {
        size_t len = 0;
        while (*cursor && *cursor != '\n' && len + 1 < sizeof(line)) {
            line[len++] = *cursor++;
        }
        if (*cursor == '\n') {
            cursor++;
        }
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        } else {
            line[len] = '\0';
        }

        if (strncmp(line, "m=", 2) == 0) {
            if (in_section && keep_section && section_builder.buffer) {
                if (current_is_audio) {
                    if (!audio_has_rtcpfb_111) {
                        sfu_builder_append(&section_builder, "a=rtcp-fb:111 transport-cc\r\n");
                    }
                    if (!audio_has_fmtp_111) {
                        sfu_builder_append(&section_builder, "a=fmtp:111 minptime=10;useinbandfec=1\r\n");
                    }
                    if (!audio_has_red_rtpmap) {
                        char red_line[64];
                        snprintf(red_line, sizeof(red_line), "a=rtpmap:63 red/48000/%u\r\n",
                                 (unsigned)STREAM_SFU_AUDIO_CHANNELS);
                        sfu_builder_append(&section_builder, red_line);
                    }
                    if (!audio_has_red_fmtp) {
                        sfu_builder_append(&section_builder, "a=fmtp:63 111/111\r\n");
                    }
                }
                if (current_is_video) {
                    sfu_append_filtered_video_section(&section_out, section_builder.buffer);
                } else {
                    sfu_builder_append(&section_out, section_builder.buffer);
                }
                sfu_builder_reset(&section_builder);
            }
            in_section = true;
            keep_section = false;
            current_mid_index = -1;
            current_is_video = false;
            current_is_audio = false;
            audio_has_fmtp_111 = false;
            audio_has_rtcpfb_111 = false;
            audio_has_red_rtpmap = false;
            audio_has_red_fmtp = false;
            if (strncmp(line + 2, "video", 5) == 0) {
                keep_section = true;
                current_is_video = true;
            } else if (strncmp(line + 2, "audio", 5) == 0 && include_audio) {
                keep_section = true;
                current_is_video = false;
                current_is_audio = true;
            }
            if (keep_section) {
                if (current_is_audio) {
                    int port = 0;
                    char proto[64] = {0};
                    if (sscanf(line, "m=audio %d %63s", &port, proto) == 2) {
                        char mline[128];
                        snprintf(mline, sizeof(mline), "m=audio %d %s 111 63\r\n", port, proto);
                        sfu_builder_append(&section_builder, mline);
                    } else {
                        sfu_builder_append(&section_builder, line);
                        sfu_builder_append(&section_builder, "\r\n");
                    }
                } else {
                    sfu_builder_append(&section_builder, line);
                    sfu_builder_append(&section_builder, "\r\n");
                }
            }
            continue;
        }

        if (!in_section) {
            if (strncmp(line, "a=group:BUNDLE", 14) == 0) {
                continue;
            }
            sfu_builder_append(&session_builder, line);
            sfu_builder_append(&session_builder, "\r\n");
            continue;
        }

        if (keep_section) {
            if (current_is_audio &&
                STREAM_SFU_AUDIO_CHANNELS == 1 &&
                strncmp(line, "a=rtpmap:", 9) == 0 &&
                strstr(line, "opus/48000/2")) {
                char mono_line[128];
                const char *colon = strchr(line, ':');
                if (colon) {
                    const char *space = strchr(colon + 1, ' ');
                    if (space) {
                        size_t prefix_len = (size_t)(space - line);
                        if (prefix_len < sizeof(mono_line)) {
                            memcpy(mono_line, line, prefix_len);
                            mono_line[prefix_len] = '\0';
                            snprintf(mono_line + prefix_len,
                                     sizeof(mono_line) - prefix_len,
                                     " opus/48000/1");
                            sfu_builder_append(&section_builder, mono_line);
                            sfu_builder_append(&section_builder, "\r\n");
                            continue;
                        }
                    }
                }
            }
            if (current_is_audio) {
                if (strncmp(line, "a=fmtp:111", 10) == 0) {
                    audio_has_fmtp_111 = true;
                } else if (strncmp(line, "a=rtcp-fb:111", 13) == 0) {
                    audio_has_rtcpfb_111 = true;
                } else if (strncmp(line, "a=rtpmap:63", 11) == 0 && strstr(line, "red/48000")) {
                    audio_has_red_rtpmap = true;
                } else if (strncmp(line, "a=fmtp:63", 9) == 0) {
                    audio_has_red_fmtp = true;
                }
            }
            if (strcmp(line, "a=sendrecv") == 0) {
                sfu_builder_append(&section_builder, "a=sendonly\r\n");
                continue;
            }
            if (strncmp(line, "a=mid:", 6) == 0) {
                if (current_mid_index < 0 && mid_count < 4) {
                    current_mid_index = (int)mid_count;
                    snprintf(mids[mid_count], sizeof(mids[mid_count]), "%u", (unsigned)mid_count);
                    mid_count++;
                }
                if (current_mid_index >= 0) {
                    char mid_line[32];
                    snprintf(mid_line, sizeof(mid_line), "a=mid:%d", current_mid_index);
                    sfu_builder_append(&section_builder, mid_line);
                    sfu_builder_append(&section_builder, "\r\n");
                } else {
                    sfu_builder_append(&section_builder, line);
                    sfu_builder_append(&section_builder, "\r\n");
                }
                continue;
            }
            sfu_builder_append(&section_builder, line);
            sfu_builder_append(&section_builder, "\r\n");
        }
    }

    if (in_section && keep_section && section_builder.buffer) {
        if (current_is_audio) {
            if (!audio_has_rtcpfb_111) {
                sfu_builder_append(&section_builder, "a=rtcp-fb:111 transport-cc\r\n");
            }
            if (!audio_has_fmtp_111) {
                sfu_builder_append(&section_builder, "a=fmtp:111 minptime=10;useinbandfec=1\r\n");
            }
            if (!audio_has_red_rtpmap) {
                char red_line[64];
                snprintf(red_line, sizeof(red_line), "a=rtpmap:63 red/48000/%u\r\n",
                         (unsigned)STREAM_SFU_AUDIO_CHANNELS);
                sfu_builder_append(&section_builder, red_line);
            }
            if (!audio_has_red_fmtp) {
                sfu_builder_append(&section_builder, "a=fmtp:63 111/111\r\n");
            }
        }
        if (current_is_video) {
            sfu_append_filtered_video_section(&section_out, section_builder.buffer);
        } else {
            sfu_builder_append(&section_out, section_builder.buffer);
        }
    }

    sfu_string_builder_t out_builder;
    sfu_builder_init(&out_builder);
    sfu_builder_append(&out_builder, session_builder.buffer ? session_builder.buffer : "");
    if (mid_count > 0) {
        sfu_builder_append(&out_builder, "a=group:BUNDLE");
        for (size_t i = 0; i < mid_count; ++i) {
            sfu_builder_append(&out_builder, " ");
            sfu_builder_append(&out_builder, mids[i]);
        }
        sfu_builder_append(&out_builder, "\r\n");
    }
    sfu_builder_append(&out_builder, section_out.buffer ? section_out.buffer : "");

    free(session_builder.buffer);
    free(section_builder.buffer);
    free(section_out.buffer);
    if (out_len) {
        *out_len = out_builder.length;
    }
    return out_builder.buffer;
}
static void sfu_track_info_reset(sfu_track_info_t *info)
{
    if (!info) {
        return;
    }
    memset(info, 0, sizeof(*info));
    info->track_type = stream_video_sfu_models_TrackType_TRACK_TYPE_UNSPECIFIED;
    info->muted = false;
    info->payload_type = -1;
    info->video_bitrate = 0;
    info->publish_option_id = 0;
}

static bool sfu_parse_sdp_tracks(const char *sdp,
                                 sfu_track_info_t *tracks,
                                 size_t max_tracks,
                                 size_t *track_count)
{
    if (!sdp || !tracks || !track_count || max_tracks == 0) {
        return false;
    }

    size_t count = 0;
    bool in_section = false;
    bool sending = false;
    int section_payload = -1;
    int h264_pt = -1;
    char h264_fmtp[128] = {0};
    bool h264_fmtp_set = false;
    sfu_track_info_t current;
    sfu_track_info_reset(&current);

    const char *cursor = sdp;
    while (*cursor) {
        char line[256] = {0};
        size_t len = 0;
        while (*cursor && *cursor != '\n' && len + 1 < sizeof(line)) {
            line[len++] = *cursor++;
        }
        if (*cursor == '\n') {
            cursor++;
        }
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }

        if (strncmp(line, "m=", 2) == 0) {
            if (in_section && sending && current.mid[0] && count < max_tracks) {
                tracks[count++] = current;
            }
            sfu_track_info_reset(&current);
            in_section = true;
            sending = false;
            section_payload = -1;
            if (strncmp(line + 2, "video", 5) == 0) {
                current.track_type = stream_video_sfu_models_TrackType_TRACK_TYPE_VIDEO;
            } else if (strncmp(line + 2, "audio", 5) == 0) {
                current.track_type = stream_video_sfu_models_TrackType_TRACK_TYPE_AUDIO;
            }
            h264_pt = -1;
            h264_fmtp[0] = '\0';
            h264_fmtp_set = false;
            const char *payload = line + 2;
            int token_index = 0;
            while (*payload) {
                while (*payload == ' ') {
                    ++payload;
                }
                if (!*payload) {
                    break;
                }
                const char *token_end = strchr(payload, ' ');
                size_t token_len = token_end ? (size_t)(token_end - payload) : strlen(payload);
                if (token_index >= 3) {
                    char tmp[8] = {0};
                    size_t copy_len = token_len < (sizeof(tmp) - 1) ? token_len : (sizeof(tmp) - 1);
                    memcpy(tmp, payload, copy_len);
                    section_payload = atoi(tmp);
                    current.payload_type = section_payload;
                    break;
                }
                token_index++;
                payload += token_len;
            }
            continue;
        }

        if (!in_section) {
            continue;
        }

        if (strncmp(line, "a=mid:", 6) == 0) {
            sfu_copy_token(current.mid, sizeof(current.mid), line + 6);
        } else if (strncmp(line, "a=msid:", 7) == 0) {
            const char *value = line + 7;
            while (*value == ' ') {
                ++value;
            }
            const char *space = strchr(value, ' ');
            if (space) {
                const char *track = space + 1;
                while (*track == ' ') {
                    ++track;
                }
                sfu_copy_token(current.track_id, sizeof(current.track_id), track);
            }
        } else if (strncmp(line, "a=rtpmap:", 9) == 0) {
            int pt = -1;
            char name[16] = {0};
            int clock = 0;
            char enc[16] = {0};
            int parsed = sscanf(line, "a=rtpmap:%d %15[^/]/%d/%15s", &pt, name, &clock, enc);
            if (parsed >= 3 && pt == section_payload) {
                strncpy(current.codec_name, name, sizeof(current.codec_name) - 1);
                current.clock_rate = (uint32_t)clock;
                if (parsed == 4) {
                    strncpy(current.encoding_params, enc, sizeof(current.encoding_params) - 1);
                }
            }
            if (parsed >= 3 && strcasecmp(name, "H264") == 0) {
                h264_pt = pt;
            }
        } else if (strncmp(line, "a=fmtp:", 7) == 0) {
            int pt = -1;
            const char *params = strchr(line, ' ');
            if (sscanf(line, "a=fmtp:%d", &pt) == 1 && pt == section_payload && params) {
                while (*params == ' ') {
                    ++params;
                }
                strncpy(current.fmtp, params, sizeof(current.fmtp) - 1);
            }
            if (h264_pt >= 0 && pt == h264_pt && params) {
                while (*params == ' ') {
                    ++params;
                }
                if (!h264_fmtp_set || strstr(params, "packetization-mode=1")) {
                    strncpy(h264_fmtp, params, sizeof(h264_fmtp) - 1);
                    h264_fmtp_set = true;
                }
            }
        } else if (strncmp(line, "a=framesize:", 12) == 0) {
            int pt = -1;
            const char *params = strchr(line, ' ');
            if (sscanf(line, "a=framesize:%d", &pt) == 1 && pt == section_payload && params) {
                while (*params == ' ') {
                    ++params;
                }
                unsigned int w = 0;
                unsigned int h = 0;
                if (sscanf(params, "%ux%u", &w, &h) == 2 || sscanf(params, "%u-%u", &w, &h) == 2) {
                    current.video_width = (uint32_t)w;
                    current.video_height = (uint32_t)h;
                }
            }
        } else if (strncmp(line, "a=framerate:", 12) == 0) {
            const char *value = line + 12;
            while (*value == ' ') {
                ++value;
            }
            current.video_fps = (uint32_t)atoi(value);
        } else if (strcmp(line, "a=sendrecv") == 0 || strcmp(line, "a=sendonly") == 0) {
            sending = true;
        } else if (strcmp(line, "a=recvonly") == 0 || strcmp(line, "a=inactive") == 0) {
            sending = false;
        }
    }

    if (in_section && sending && current.mid[0] && count < max_tracks) {
        if (current.track_type == stream_video_sfu_models_TrackType_TRACK_TYPE_VIDEO &&
            h264_fmtp_set) {
            strncpy(current.codec_name, "H264", sizeof(current.codec_name) - 1);
            strncpy(current.fmtp, h264_fmtp, sizeof(current.fmtp) - 1);
            current.payload_type = h264_pt;
        }
        tracks[count++] = current;
    }

    *track_count = count;
    return count > 0;
}

static void sfu_fill_uuid(char *buffer, size_t capacity)
{
    if (!buffer || capacity < 37) {
        if (buffer && capacity) {
            buffer[0] = '\0';
        }
        return;
    }
    uint8_t bytes[16];
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = (uint8_t)(esp_random() & 0xFF);
    }
    bytes[6] = (uint8_t)((bytes[6] & 0x0F) | 0x40);
    bytes[8] = (uint8_t)((bytes[8] & 0x3F) | 0x80);
    snprintf(buffer, capacity,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3],
             bytes[4], bytes[5],
             bytes[6], bytes[7],
             bytes[8], bytes[9],
             bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
}

static void sfu_reset_publish_track_ids(stream_sfu_client_handle_t client)
{
    if (!client) {
        return;
    }
    memset(client->publish_track_ids, 0, sizeof(client->publish_track_ids));
    client->publish_track_id_count = 0;
}

static void sfu_reset_publish_options(stream_sfu_client_handle_t client)
{
    if (!client) {
        return;
    }
    memset(client->publish_options, 0, sizeof(client->publish_options));
    client->publish_option_count = 0;
}

static void sfu_assign_track_id(stream_sfu_client_handle_t client, sfu_track_info_t *track)
{
    if (!track) {
        return;
    }
    if (!track->track_id[0]) {
        sfu_fill_uuid(track->track_id, sizeof(track->track_id));
        return;
    }
    return;

}

static const sfu_publish_option_t *sfu_find_publish_option(
    stream_sfu_client_handle_t client,
    stream_video_sfu_models_TrackType track_type,
    const char *codec_name)
{
    if (!client) {
        return NULL;
    }
    for (size_t i = 0; i < client->publish_option_count; ++i) {
        if (client->publish_options[i].track_type != track_type) {
            continue;
        }
        if (codec_name && codec_name[0] && client->publish_options[i].codec_name[0]) {
            if (strcasecmp(client->publish_options[i].codec_name, codec_name) != 0) {
                continue;
            }
        }
        return &client->publish_options[i];
    }
    return NULL;
}

static bool decode_publish_options_cb(pb_istream_t *stream,
                                      const pb_field_t *field,
                                      void **arg)
{
    (void)field;
    sfu_publish_options_ctx_t *ctx = (sfu_publish_options_ctx_t *)(*arg);
    if (!ctx || !ctx->options || ctx->capacity == 0) {
        return false;
    }

    ESP_LOGI(TAG, "Publish option bytes: %u", (unsigned)stream->bytes_left);
    stream_video_sfu_models_PublishOption option = stream_video_sfu_models_PublishOption_init_zero;
    char codec_buf[32] = {0};
    char codec_fmtp[64] = {0};
    char codec_params[32] = {0};
    sfu_string_buffer_t codec_ctx = { .buffer = codec_buf, .capacity = sizeof(codec_buf) };
    sfu_string_buffer_t fmtp_ctx = { .buffer = codec_fmtp, .capacity = sizeof(codec_fmtp) };
    sfu_string_buffer_t params_ctx = { .buffer = codec_params, .capacity = sizeof(codec_params) };
    option.codec.name.funcs.decode = decode_string_cb;
    option.codec.name.arg = &codec_ctx;
    option.codec.fmtp.funcs.decode = decode_string_cb;
    option.codec.fmtp.arg = &fmtp_ctx;
    option.codec.encoding_parameters.funcs.decode = decode_string_cb;
    option.codec.encoding_parameters.arg = &params_ctx;

    if (!pb_decode(stream, stream_video_sfu_models_PublishOption_fields, &option)) {
        ESP_LOGW(TAG, "Failed to decode publish option: %s", PB_GET_ERROR(stream));
        return false;
    }

    if (ctx->count >= ctx->capacity) {
        return true;
    }

    sfu_publish_option_t *dest = &ctx->options[ctx->count++];
    dest->track_type = option.track_type;
    dest->id = option.id;
    dest->fps = (uint32_t)option.fps;
    dest->width = option.video_dimension.width;
    dest->height = option.video_dimension.height;
    if (codec_buf[0]) {
        strncpy(dest->codec_name, codec_buf, sizeof(dest->codec_name) - 1);
    }
    ESP_LOGI(TAG, "Decoded publish option: id=%d type=%d codec=%s %ux%u@%u fmtp=%s",
             (int)dest->id,
             (int)dest->track_type,
             dest->codec_name[0] ? dest->codec_name : "(none)",
             (unsigned)dest->width,
             (unsigned)dest->height,
             (unsigned)dest->fps,
             codec_fmtp[0] ? codec_fmtp : "(none)");
    return true;
}

static bool encode_layers_cb(pb_ostream_t *stream,
                             const pb_field_t *field,
                             void * const *arg)
{
    const sfu_video_layer_info_t *layer_info = (const sfu_video_layer_info_t *)(*arg);
    if (!layer_info || layer_info->width == 0 || layer_info->height == 0) {
        return true;
    }

    stream_video_sfu_models_VideoDimension dimension = stream_video_sfu_models_VideoDimension_init_zero;
    dimension.width = layer_info->width;
    dimension.height = layer_info->height;

    stream_video_sfu_models_VideoLayer layer = stream_video_sfu_models_VideoLayer_init_zero;
    if (layer_info->rid && layer_info->rid[0]) {
        layer.rid.funcs.encode = encode_string_cb;
        layer.rid.arg = (void *)layer_info->rid;
    }
    layer.has_video_dimension = true;
    layer.video_dimension = dimension;
    layer.fps = layer_info->fps;
    layer.bitrate = layer_info->bitrate;
    layer.quality = stream_video_sfu_models_VideoQuality_VIDEO_QUALITY_HIGH;

    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }
    return pb_encode_submessage(stream, stream_video_sfu_models_VideoLayer_fields, &layer);
}

static bool encode_tracks_cb(pb_ostream_t *stream,
                             const pb_field_t *field,
                             void * const *arg)
{
    const sfu_track_list_t *list = (const sfu_track_list_t *)(*arg);
    if (!list || !list->tracks) {
        return true;
    }
    for (size_t i = 0; i < list->count; ++i) {
        stream_video_sfu_models_TrackInfo track = stream_video_sfu_models_TrackInfo_init_zero;
        track.track_id.funcs.encode = encode_string_cb;
        track.track_id.arg = (void *)list->tracks[i].track_id;
        track.track_type = list->tracks[i].track_type;
        track.mid.funcs.encode = encode_string_cb;
        track.mid.arg = (void *)list->tracks[i].mid;
        track.muted = list->tracks[i].muted;
        track.stereo = false;
        track.publish_option_id = list->tracks[i].publish_option_id;

        stream_video_sfu_models_Codec codec = stream_video_sfu_models_Codec_init_zero;
        if (list->tracks[i].payload_type >= 0) {
            codec.payload_type = (uint32_t)list->tracks[i].payload_type;
        }
        if (list->tracks[i].codec_name[0]) {
            codec.name.funcs.encode = encode_string_cb;
            codec.name.arg = (void *)list->tracks[i].codec_name;
        }
        if (list->tracks[i].clock_rate > 0) {
            codec.clock_rate = list->tracks[i].clock_rate;
        }
        if (list->tracks[i].encoding_params[0]) {
            codec.encoding_parameters.funcs.encode = encode_string_cb;
            codec.encoding_parameters.arg = (void *)list->tracks[i].encoding_params;
        }
        if (list->tracks[i].fmtp[0]) {
            codec.fmtp.funcs.encode = encode_string_cb;
            codec.fmtp.arg = (void *)list->tracks[i].fmtp;
        }
        if (list->tracks[i].codec_name[0] || list->tracks[i].fmtp[0]) {
            track.has_codec = true;
        }
        track.codec = codec;

        sfu_video_layer_info_t layer_info = {
            .width = list->tracks[i].video_width,
            .height = list->tracks[i].video_height,
            .fps = list->tracks[i].video_fps,
            .bitrate = list->tracks[i].video_bitrate,
            .rid = list->tracks[i].video_rid,
        };
        if (track.track_type == stream_video_sfu_models_TrackType_TRACK_TYPE_VIDEO &&
            layer_info.width > 0 && layer_info.height > 0) {
            track.layers.funcs.encode = encode_layers_cb;
            track.layers.arg = &layer_info;
        }

        if (!pb_encode_tag_for_field(stream, field)) {
            return false;
        }
        if (!pb_encode_submessage(stream, stream_video_sfu_models_TrackInfo_fields, &track)) {
            return false;
        }
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
                ESP_LOGW(TAG, "No SFU events for 15s, triggering reconnect...");
                esp_websocket_client_stop(client->ws_client);
                client->last_event_tick = xTaskGetTickCount();
            }
        }
    }

    vTaskDelete(NULL);
}

/**
 * @brief SFU reconnect task: waits for disconnect then reconnects with backoff
 */
static void sfu_reconnect_task(void *pvParameters)
{
    stream_sfu_client_handle_t client = (stream_sfu_client_handle_t)pvParameters;
    const EventBits_t wait_bits = SFU_DISCONNECTED_BIT | SFU_ERROR_BIT;
    const TickType_t poll_timeout = pdMS_TO_TICKS(500);

    while (client->should_reconnect) {
        EventBits_t bits = xEventGroupWaitBits(client->event_group, wait_bits,
                                              false, false, poll_timeout);
        if (!client->should_reconnect) {
            break;
        }
        if ((bits & wait_bits) == 0) {
            continue;
        }

        if (esp_websocket_client_is_connected(client->ws_client)) {
            ESP_LOGI(TAG, "SFU WS already connected, clearing disconnect/error bits");
            xEventGroupClearBits(client->event_group, wait_bits);
            client->reconnect_attempts = 0;
            continue;
        }

        if (client->config.reconnect_max_attempts > 0 &&
            client->reconnect_attempts >= client->config.reconnect_max_attempts) {
            ESP_LOGW(TAG, "SFU max reconnection attempts reached");
            break;
        }

        client->reconnect_attempts++;
        uint32_t interval_ms = client->config.reconnect_interval_ms > 0
                               ? client->config.reconnect_interval_ms
                               : 5000;
        ESP_LOGI(TAG, "SFU reconnecting in %lu ms (attempt %lu)...",
                 (unsigned long)interval_ms, (unsigned long)client->reconnect_attempts);
        vTaskDelay(pdMS_TO_TICKS(interval_ms));

        if (!client->should_reconnect) {
            break;
        }

        if (esp_websocket_client_is_connected(client->ws_client)) {
            ESP_LOGI(TAG, "SFU WS reconnected during backoff, skipping start");
            xEventGroupClearBits(client->event_group, wait_bits);
            client->reconnect_attempts = 0;
            continue;
        }

        xEventGroupClearBits(client->event_group, wait_bits);
        client->state = STREAM_SFU_STATE_CONNECTING;

        esp_websocket_client_stop(client->ws_client);
        esp_err_t err = esp_websocket_client_start(client->ws_client);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "SFU reconnect start failed: %s", esp_err_to_name(err));
            xEventGroupSetBits(client->event_group, SFU_ERROR_BIT);
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
        .buffer_size = SFU_HTTP_BUFFER_SIZE,
        .buffer_size_tx = SFU_HTTP_BUFFER_SIZE,
    };
    esp_http_client_handle_t http = esp_http_client_init(&config);
    if (!http) {
        return STREAM_VIDEO_ERR_FAIL;
    }

    esp_http_client_set_method(http, HTTP_METHOD_POST);
    esp_http_client_set_header(http, "Authorization", client->http_auth_header);
    esp_http_client_set_header(http, "Content-Type", "application/protobuf");
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

static stream_video_error_t sfu_send_signal_request_with_response(
    stream_sfu_client_handle_t client,
    const char *path,
    const uint8_t *payload,
    size_t payload_len,
    uint8_t **response_out,
    size_t *response_len_out)
{
    if (!client || !client->http_url[0] || !path || !payload || payload_len == 0 ||
        !response_out || !response_len_out) {
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
        .buffer_size = SFU_HTTP_BUFFER_SIZE,
        .buffer_size_tx = SFU_HTTP_BUFFER_SIZE,
    };
    esp_http_client_handle_t http = esp_http_client_init(&config);
    if (!http) {
        return STREAM_VIDEO_ERR_FAIL;
    }

    esp_http_client_set_method(http, HTTP_METHOD_POST);
    esp_http_client_set_header(http, "Authorization", client->http_auth_header);
    esp_http_client_set_header(http, "Content-Type", "application/protobuf");
    esp_http_client_set_header(http, "Accept", "application/protobuf");

    ESP_LOGI(TAG, "SFU HTTP request: POST %s (%u bytes)", url, (unsigned)payload_len);

    esp_err_t err = esp_http_client_open(http, payload_len);
    if (err != ESP_OK) {
        esp_http_client_cleanup(http);
        return STREAM_VIDEO_ERR_NETWORK;
    }
    int written = esp_http_client_write(http, (const char *)payload, payload_len);
    if (written < 0) {
        esp_http_client_close(http);
        esp_http_client_cleanup(http);
        return STREAM_VIDEO_ERR_NETWORK;
    }

    int status_code = esp_http_client_fetch_headers(http);
    int content_len = esp_http_client_get_content_length(http);
    (void)status_code;

    size_t buf_size = content_len > 0 ? (size_t)content_len : 2048;
    uint8_t *buffer = (uint8_t *)malloc(buf_size);
    if (!buffer) {
        esp_http_client_close(http);
        esp_http_client_cleanup(http);
        return STREAM_VIDEO_ERR_NO_MEM;
    }

    size_t total = 0;
    while (1) {
        if (total == buf_size) {
            size_t new_size = buf_size * 2;
            uint8_t *tmp = (uint8_t *)realloc(buffer, new_size);
            if (!tmp) {
                free(buffer);
                esp_http_client_close(http);
                esp_http_client_cleanup(http);
                return STREAM_VIDEO_ERR_NO_MEM;
            }
            buffer = tmp;
            buf_size = new_size;
        }
        int read = esp_http_client_read(http, (char *)buffer + total, buf_size - total);
        if (read <= 0) {
            break;
        }
        total += (size_t)read;
    }

    int http_status = esp_http_client_get_status_code(http);
    esp_http_client_close(http);
    esp_http_client_cleanup(http);

    ESP_LOGI(TAG, "SFU HTTP response: status=%d body=%u bytes", http_status, (unsigned)total);
    if (total >= 4) {
        ESP_LOGI(TAG, "SFU HTTP response first bytes: %02x %02x %02x %02x",
                 buffer[0], buffer[1], buffer[2], buffer[3]);
    }

    if (http_status < 200 || http_status >= 300) {
        ESP_LOGE(TAG, "SFU signal request failed: HTTP %d (body=%u bytes)", http_status, (unsigned)total);
        if (total > 0 && total < 256) {
            ESP_LOGE(TAG, "SFU error body: %.*s", (int)total, (char *)buffer);
        }
        free(buffer);
        return STREAM_VIDEO_ERR_NETWORK;
    }

    *response_out = buffer;
    *response_len_out = total;
    return STREAM_VIDEO_ERR_OK;
}

static stream_video_error_t sfu_send_set_publisher_http(
    stream_sfu_client_handle_t client,
    const char *sdp,
    bool *out_should_retry)
{
    if (out_should_retry) {
        *out_should_retry = false;
    }
    if (!client || !sdp || !client->join_session_id[0]) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }

    uint8_t buffer[4096];
    size_t filtered_len = 0;
    char *filtered_sdp = sfu_filter_sdp(sdp, client->publish_audio, &filtered_len);
    if (!filtered_sdp) {
        return STREAM_VIDEO_ERR_FAIL;
    }

    stream_video_sfu_signal_SetPublisherRequest request =
        stream_video_sfu_signal_SetPublisherRequest_init_zero;
    request.sdp.funcs.encode = encode_string_cb;
    request.sdp.arg = (void *)filtered_sdp;
    request.session_id.funcs.encode = encode_string_cb;
    request.session_id.arg = (void *)client->join_session_id;

    sfu_track_info_t track_infos[2];
    size_t track_count = 0;
    bool parsed = sfu_parse_sdp_tracks(filtered_sdp, track_infos, 2, &track_count);
    if (!parsed || track_count == 0) {
        if (client->publish_video) {
            sfu_track_info_t fallback;
            sfu_track_info_reset(&fallback);
            fallback.track_type = stream_video_sfu_models_TrackType_TRACK_TYPE_VIDEO;
            strncpy(fallback.mid, "0", sizeof(fallback.mid) - 1);
            track_infos[0] = fallback;
            track_count = 1;
            ESP_LOGW(TAG, "Failed to parse SDP tracks, using fallback mid");
        } else {
            free(filtered_sdp);
            ESP_LOGE(TAG, "No publishable tracks found in SDP");
            return STREAM_VIDEO_ERR_FAIL;
        }
    }

    for (size_t i = 0; i < track_count; ++i) {
        sfu_assign_track_id(client, &track_infos[i]);
        if (track_infos[i].track_type == stream_video_sfu_models_TrackType_TRACK_TYPE_AUDIO) {
            track_infos[i].muted = !client->publish_audio;
        } else if (track_infos[i].track_type == stream_video_sfu_models_TrackType_TRACK_TYPE_VIDEO) {
            track_infos[i].muted = !client->publish_video;
        }

        const sfu_publish_option_t *opt = sfu_find_publish_option(
            client,
            track_infos[i].track_type,
            track_infos[i].codec_name);
        if (opt) {
            track_infos[i].publish_option_id = opt->id;
            if (!track_infos[i].codec_name[0] && opt->codec_name[0]) {
                strncpy(track_infos[i].codec_name, opt->codec_name, sizeof(track_infos[i].codec_name) - 1);
            }
            if (track_infos[i].video_width == 0 && opt->width) {
                track_infos[i].video_width = opt->width;
            }
            if (track_infos[i].video_height == 0 && opt->height) {
                track_infos[i].video_height = opt->height;
            }
            if (track_infos[i].video_fps == 0 && opt->fps) {
                track_infos[i].video_fps = opt->fps;
            }
        }

        if (track_infos[i].track_type == stream_video_sfu_models_TrackType_TRACK_TYPE_VIDEO) {
            if (!track_infos[i].video_rid[0]) {
                strncpy(track_infos[i].video_rid, "f", sizeof(track_infos[i].video_rid) - 1);
            }
            if (track_infos[i].video_width == 0) {
#ifdef CONFIG_STREAM_VIDEO_WIDTH
                track_infos[i].video_width = CONFIG_STREAM_VIDEO_WIDTH;
#else
                track_infos[i].video_width = 160;
#endif
            }
            if (track_infos[i].video_height == 0) {
#ifdef CONFIG_STREAM_VIDEO_HEIGHT
                track_infos[i].video_height = CONFIG_STREAM_VIDEO_HEIGHT;
#else
                track_infos[i].video_height = 120;
#endif
            }
            if (track_infos[i].video_fps == 0) {
#ifdef CONFIG_STREAM_VIDEO_FPS
                track_infos[i].video_fps = CONFIG_STREAM_VIDEO_FPS;
#else
                track_infos[i].video_fps = 10;
#endif
            }
            if (track_infos[i].video_bitrate == 0) {
#ifdef CONFIG_STREAM_VIDEO_BITRATE
                track_infos[i].video_bitrate = CONFIG_STREAM_VIDEO_BITRATE;
#else
                track_infos[i].video_bitrate = 150000;
#endif
            }
        }
    }

    sfu_track_list_t track_list = {
        .tracks = track_infos,
        .count = track_count,
    };
    request.tracks.funcs.encode = encode_tracks_cb;
    request.tracks.arg = &track_list;

    char *escaped_sdp = sfu_escape_json_string(filtered_sdp);
    if (escaped_sdp) {
        size_t json_cap = strlen(escaped_sdp) + 1024 + track_count * 256;
        char *json = (char *)malloc(json_cap);
        if (json) {
            size_t pos = 0;
            pos += snprintf(json + pos, json_cap - pos,
                            "{\"sdp\":\"%s\",\"session_id\":\"%s\",\"tracks\":[",
                            escaped_sdp,
                            client->join_session_id);
            for (size_t i = 0; i < track_count; ++i) {
                const sfu_track_info_t *t = &track_infos[i];
                if (i > 0) {
                    pos += snprintf(json + pos, json_cap - pos, ",");
                }
                pos += snprintf(json + pos, json_cap - pos,
                                "{\"track_id\":\"%s\",\"track_type\":\"%s\",\"mid\":\"%s\","
                                "\"muted\":%s,\"codec\":{\"name\":\"%s\",\"fmtp\":\"%s\"},"
                                "\"layers\":[{\"rid\":\"%s\",\"video_dimension\":{\"width\":%u,\"height\":%u},"
                                "\"bitrate\":%u,\"fps\":%u}]}",
                                t->track_id,
                                sfu_track_type_to_str(t->track_type),
                                t->mid,
                                t->muted ? "true" : "false",
                                t->codec_name,
                                t->fmtp,
                                t->video_rid[0] ? t->video_rid : "",
                                (unsigned)t->video_width,
                                (unsigned)t->video_height,
                                (unsigned)t->video_bitrate,
                                (unsigned)t->video_fps);
            }
            snprintf(json + pos, json_cap - pos, "]}");
            ESP_LOGI(TAG, "SetPublisher request JSON: %s", json);
            free(json);
        }
        free(escaped_sdp);
    }

    ESP_LOGI(TAG, "SetPublisher request: session_id=%s tracks=%u",
             client->join_session_id, (unsigned)track_count);
    for (size_t i = 0; i < track_count; ++i) {
        ESP_LOGI(TAG, "SetPublisher track[%u]: id=%s mid=%s type=%d muted=%d codec=%s pt=%d fps=%u option_id=%d size=%ux%u rid=%s bitrate=%u",
                 (unsigned)i,
                 track_infos[i].track_id[0] ? track_infos[i].track_id : "(empty)",
                 track_infos[i].mid[0] ? track_infos[i].mid : "(empty)",
                 (int)track_infos[i].track_type,
                 (int)track_infos[i].muted,
                 track_infos[i].codec_name[0] ? track_infos[i].codec_name : "(none)",
                 track_infos[i].payload_type,
                 (unsigned)track_infos[i].video_fps,
                 (int)track_infos[i].publish_option_id,
                 (unsigned)track_infos[i].video_width,
                 (unsigned)track_infos[i].video_height,
                 track_infos[i].video_rid[0] ? track_infos[i].video_rid : "(none)",
                 (unsigned)track_infos[i].video_bitrate);
    }

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, stream_video_sfu_signal_SetPublisherRequest_fields, &request)) {
        free(filtered_sdp);
        ESP_LOGE(TAG, "Failed to encode SetPublisherRequest");
        return STREAM_VIDEO_ERR_FAIL;
    }

    bool ws_connected = client->ws_client && esp_websocket_client_is_connected(client->ws_client);
    TickType_t pre_tick = xTaskGetTickCount();
    uint32_t since_ws_connect_ms = 0;
    if (client->ws_connected_tick) {
        since_ws_connect_ms = (uint32_t)((pre_tick - client->ws_connected_tick) * portTICK_PERIOD_MS);
    }
    uint32_t since_last_event_ms = (uint32_t)((pre_tick - client->last_event_tick) * portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "SetPublisher pre-flight: ws_connected=%d, sfu_state=%d, "
             "session_id=%s, ws_uptime=%lu ms, last_sfu_event=%lu ms ago",
             (int)ws_connected,
             (int)client->state,
             client->join_session_id,
             (unsigned long)since_ws_connect_ms,
             (unsigned long)since_last_event_ms);

    uint8_t *resp = NULL;
    size_t resp_len = 0;
    stream_video_error_t err = sfu_send_signal_request_with_response(
        client,
        "stream.video.sfu.signal.SignalServer/SetPublisher",
        buffer,
        stream.bytes_written,
        &resp,
        &resp_len);

    bool ws_connected_after = client->ws_client && esp_websocket_client_is_connected(client->ws_client);
    if (ws_connected != ws_connected_after) {
        ESP_LOGW(TAG, "SetPublisher: WS state changed during HTTP request! before=%d after=%d",
                 (int)ws_connected, (int)ws_connected_after);
    }

    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "SetPublisher HTTP request failed: err=%d, ws_connected=%d (was %d)",
                 (int)err, (int)ws_connected_after, (int)ws_connected);
        free(filtered_sdp);
        return err;
    }

    char answer_buf[4096] = {0};
    char session_buf[128] = {0};
    char error_buf[256] = {0};
    sfu_string_buffer_t answer_ctx = { .buffer = answer_buf, .capacity = sizeof(answer_buf) };
    sfu_string_buffer_t session_ctx = { .buffer = session_buf, .capacity = sizeof(session_buf) };
    sfu_string_buffer_t error_ctx = { .buffer = error_buf, .capacity = sizeof(error_buf) };
    stream_video_sfu_signal_SetPublisherResponse response =
        stream_video_sfu_signal_SetPublisherResponse_init_zero;
    response.sdp.funcs.decode = decode_string_cb;
    response.sdp.arg = &answer_ctx;
    response.session_id.funcs.decode = decode_string_cb;
    response.session_id.arg = &session_ctx;
    response.error.message.funcs.decode = decode_string_cb;
    response.error.message.arg = &error_ctx;

    pb_istream_t istream = pb_istream_from_buffer(resp, resp_len);
    if (!pb_decode(&istream, stream_video_sfu_signal_SetPublisherResponse_fields, &response)) {
        ESP_LOGE(TAG, "Failed to decode SetPublisherResponse");
        free(resp);
        free(filtered_sdp);
        return STREAM_VIDEO_ERR_FAIL;
    }
    free(resp);

    if (error_buf[0] || (response.has_error &&
        response.error.code != stream_video_sfu_models_ErrorCode_ERROR_CODE_UNSPECIFIED)) {
        bool ws_now = client->ws_client && esp_websocket_client_is_connected(client->ws_client);
        ESP_LOGE(TAG, "SetPublisher error: code=%d should_retry=%d msg=%s",
                 (int)response.error.code,
                 (int)response.error.should_retry,
                 error_buf[0] ? error_buf : "(none)");
        ESP_LOGE(TAG, "SetPublisher diagnosis: ws_connected_before=%d, ws_connected_now=%d, "
                 "session_id=%s, sfu_state=%d",
                 (int)ws_connected, (int)ws_now,
                 client->join_session_id,
                 (int)client->state);
        if (out_should_retry) {
            *out_should_retry = response.error.should_retry;
        }
        free(filtered_sdp);
        return STREAM_VIDEO_ERR_FAIL;
    }

    char *escaped_answer = sfu_escape_json_string(answer_buf);
    char *escaped_session = sfu_escape_json_string(session_buf);
    char *escaped_error = sfu_escape_json_string(error_buf);
    if (escaped_answer && escaped_session && escaped_error) {
        ESP_LOGI(TAG,
                 "SetPublisher response JSON: {\"sdp\":\"%s\",\"session_id\":\"%s\",\"ice_restart\":%s,\"error\":%s}",
                 escaped_answer,
                 escaped_session,
                 response.ice_restart ? "true" : "false",
                 error_buf[0] ? "\"Invalid SetPublisher request\"" : "null");
    }
    if (escaped_answer) {
        free(escaped_answer);
    }
    if (escaped_session) {
        free(escaped_session);
    }
    if (escaped_error) {
        free(escaped_error);
    }

    ESP_LOGI(TAG, "SetPublisher response: sdp_len=%u session_id=%s ice_restart=%d",
             (unsigned)answer_ctx.length,
             session_buf[0] ? session_buf : "(empty)",
             (int)response.ice_restart);

    if (answer_buf[0] && client->pub_peer) {
        esp_peer_msg_t msg = {
            .type = ESP_PEER_MSG_TYPE_SDP,
            .data = (uint8_t *)answer_buf,
            .size = (int)strlen(answer_buf),
        };
        esp_peer_send_msg(client->pub_peer, &msg);
        ESP_LOGI(TAG, "Publisher answer applied");
    }
    sfu_send_trickle_from_sdp(client, filtered_sdp);
    free(filtered_sdp);
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
    const char *payload = candidate;
    char json_candidate[512];
    if (candidate[0] != '{') {
        snprintf(json_candidate, sizeof(json_candidate),
                 "{\"sdpMid\":\"0\",\"sdpMLineIndex\":0,\"candidate\":\"%s\",\"usernameFragment\":\"\"}",
                 candidate);
        payload = json_candidate;
    }
    ESP_LOGI(TAG, "ICETrickle send: peer=%d candidate_json=%s",
             (int)peer_type,
             payload);
    stream_video_sfu_signal_ICETrickle request =
        stream_video_sfu_signal_ICETrickle_init_zero;

    request.peer_type = peer_type;
    request.ice_candidate.funcs.encode = encode_string_cb;
    request.ice_candidate.arg = (void *)payload;
    request.session_id.funcs.encode = encode_string_cb;
    request.session_id.arg = (void *)client->join_session_id;

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, stream_video_sfu_signal_ICETrickle_fields, &request)) {
        ESP_LOGE(TAG, "Failed to encode ICETrickle");
        return STREAM_VIDEO_ERR_FAIL;
    }

    uint8_t *resp = NULL;
    size_t resp_len = 0;
    stream_video_error_t err = sfu_send_signal_request_with_response(
        client,
        "stream.video.sfu.signal.SignalServer/IceTrickle",
        buffer,
        stream.bytes_written,
        &resp,
        &resp_len);
    if (err != STREAM_VIDEO_ERR_OK) {
        return err;
    }

    char err_buf[256] = {0};
    sfu_string_buffer_t err_ctx = { .buffer = err_buf, .capacity = sizeof(err_buf) };
    stream_video_sfu_signal_ICETrickleResponse response =
        stream_video_sfu_signal_ICETrickleResponse_init_zero;
    response.error.message.funcs.decode = decode_string_cb;
    response.error.message.arg = &err_ctx;

    pb_istream_t istream = pb_istream_from_buffer(resp, resp_len);
    if (!pb_decode(&istream, stream_video_sfu_signal_ICETrickleResponse_fields, &response)) {
        ESP_LOGE(TAG, "Failed to decode ICETrickleResponse");
        free(resp);
        return STREAM_VIDEO_ERR_FAIL;
    }
    free(resp);

    if (err_buf[0] ||
        response.error.code != stream_video_sfu_models_ErrorCode_ERROR_CODE_UNSPECIFIED) {
        ESP_LOGW(TAG, "ICETrickle error: code=%d message=%s",
                 (int)response.error.code,
                 err_buf[0] ? err_buf : "(empty)");
    } else {
        ESP_LOGI(TAG, "ICETrickle ok: peer=%d", (int)peer_type);
    }
    return STREAM_VIDEO_ERR_OK;
}

static const char *sfu_peer_state_to_str(esp_peer_state_t state)
{
    switch (state) {
        case ESP_PEER_STATE_CLOSED:
            return "CLOSED";
        case ESP_PEER_STATE_DISCONNECTED:
            return "DISCONNECTED";
        case ESP_PEER_STATE_NEW_CONNECTION:
            return "NEW_CONNECTION";
        case ESP_PEER_STATE_PAIRING:
            return "PAIRING";
        case ESP_PEER_STATE_PAIRED:
            return "PAIRED";
        case ESP_PEER_STATE_CONNECTING:
            return "CONNECTING";
        case ESP_PEER_STATE_CONNECTED:
            return "CONNECTED";
        case ESP_PEER_STATE_CONNECT_FAILED:
            return "CONNECT_FAILED";
        case ESP_PEER_STATE_DATA_CHANNEL_CONNECTED:
            return "DATA_CHANNEL_CONNECTED";
        case ESP_PEER_STATE_DATA_CHANNEL_OPENED:
            return "DATA_CHANNEL_OPENED";
        case ESP_PEER_STATE_DATA_CHANNEL_CLOSED:
            return "DATA_CHANNEL_CLOSED";
        case ESP_PEER_STATE_DATA_CHANNEL_DISCONNECTED:
            return "DATA_CHANNEL_DISCONNECTED";
        default:
            return "UNKNOWN";
    }
}

static int peer_state_handler_common(esp_peer_state_t state, void *ctx, const char *label)
{
    stream_sfu_client_handle_t client = (stream_sfu_client_handle_t)ctx;
    const char *state_str = sfu_peer_state_to_str(state);

    if (state == ESP_PEER_STATE_CONNECT_FAILED) {
        if (client) {
            ESP_LOGE(TAG, "%s peer state changed: %s (%d) (pub=%p sub=%p)",
                     label, state_str, (int)state,
                     (void *)client->pub_peer, (void *)client->peer);
        } else {
            ESP_LOGE(TAG, "%s peer state changed: %s (%d)", label, state_str, (int)state);
        }
    } else if (state == ESP_PEER_STATE_DISCONNECTED || state == ESP_PEER_STATE_CLOSED) {
        if (client) {
            ESP_LOGW(TAG, "%s peer state changed: %s (%d) (pub=%p sub=%p)",
                     label, state_str, (int)state,
                     (void *)client->pub_peer, (void *)client->peer);
        } else {
            ESP_LOGW(TAG, "%s peer state changed: %s (%d)", label, state_str, (int)state);
        }
    } else {
        if (client) {
            ESP_LOGI(TAG, "%s peer state changed: %s (%d) (pub=%p sub=%p)",
                     label, state_str, (int)state,
                     (void *)client->pub_peer, (void *)client->peer);
        } else {
            ESP_LOGI(TAG, "%s peer state changed: %s (%d)", label, state_str, (int)state);
        }
    }
    return 0;
}

static stream_video_error_t sfu_pub_peer_ensure(stream_sfu_client_handle_t client);

static void sfu_pub_peer_rebuild(stream_sfu_client_handle_t client)
{
    if (!client) {
        return;
    }
    if (client->state != STREAM_SFU_STATE_CONNECTED) {
        ESP_LOGW(TAG, "Skipping publisher rebuild — SFU not connected (state=%d)", (int)client->state);
        return;
    }

    ESP_LOGW(TAG, "Rebuilding publisher peer connection");

    if (client->pub_peer) {
        esp_peer_handle_t old_pub = client->pub_peer;
        TaskHandle_t old_task = client->pub_task;
        client->pub_peer = NULL;
        client->pub_task = NULL;
        if (old_task) {
            vTaskDelete(old_task);
        }
        esp_peer_close(old_pub);
        vTaskDelay(pdMS_TO_TICKS(100));
    } else if (client->pub_task) {
        vTaskDelete(client->pub_task);
        client->pub_task = NULL;
    }
    sfu_reset_publish_track_ids(client);

    stream_video_error_t err = sfu_pub_peer_ensure(client);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to recreate publisher peer: %d", (int)err);
    }
}

static int peer_state_handler_pub(esp_peer_state_t state, void *ctx)
{
    stream_sfu_client_handle_t client = (stream_sfu_client_handle_t)ctx;

    if (!client || !client->pub_peer) {
        ESP_LOGD(TAG, "Publisher state callback ignored — peer already torn down (state=%d)", (int)state);
        return 0;
    }

    if (state == ESP_PEER_STATE_CONNECT_FAILED) {
        ESP_LOGE(TAG, "Publisher CONNECT_FAILED: ICE unrecoverable — will tear down and recreate peer");
        sfu_pub_peer_rebuild(client);
    }

    return peer_state_handler_common(state, ctx, "Publisher");
}

static int peer_state_handler_sub(esp_peer_state_t state, void *ctx)
{
    return peer_state_handler_common(state, ctx, "Subscriber");
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
        ESP_LOGI(TAG, "Peer ICE candidate: %.*s", info->size, (const char *)info->data);
        sfu_send_ice_trickle_http(client, stream_video_sfu_PeerType_PEER_TYPE_SUBSCRIBER, (const char *)info->data);
    }

    return 0;
}

static int peer_msg_handler_pub(esp_peer_msg_t *info, void *ctx)
{
    stream_sfu_client_handle_t client = (stream_sfu_client_handle_t)ctx;
    if (!client || !info || !info->data) {
        return 0;
    }

    if (!client->pub_peer) {
        ESP_LOGW(TAG, "Publisher msg callback ignored — peer already torn down (type=%d)", (int)info->type);
        return 0;
    }

    if (info->type == ESP_PEER_MSG_TYPE_SDP) {
        ESP_LOGI(TAG, "Publisher generated SDP offer (%d bytes)", info->size);
        bool should_retry = false;
        stream_video_error_t pub_err = sfu_send_set_publisher_http(client, (const char *)info->data, &should_retry);
        if (pub_err == STREAM_VIDEO_ERR_OK) {
            client->publisher_ready = true;
            ESP_LOGI(TAG, "✓ SetPublisher succeeded, publisher_ready=true");
        } else if (should_retry) {
            for (int retry = 0; retry < 3; retry++) {
                int delay_ms = 500 + retry * 1500;
                ESP_LOGW(TAG, "SetPublisher failed (should_retry=true), retry %d/3 after %dms",
                         retry + 1, delay_ms);
                vTaskDelay(pdMS_TO_TICKS(delay_ms));
                if (!client->pub_peer || !esp_websocket_client_is_connected(client->ws_client)) {
                    ESP_LOGW(TAG, "SetPublisher retry aborted: peer or WS gone");
                    break;
                }
                should_retry = false;
                pub_err = sfu_send_set_publisher_http(client, (const char *)info->data, &should_retry);
                if (pub_err == STREAM_VIDEO_ERR_OK) {
                    client->publisher_ready = true;
                    ESP_LOGI(TAG, "✓ SetPublisher succeeded on retry %d, publisher_ready=true", retry + 1);
                    break;
                }
                if (!should_retry) {
                    ESP_LOGE(TAG, "SetPublisher failed with non-retryable error, giving up");
                    break;
                }
            }
        } else {
            ESP_LOGE(TAG, "SetPublisher failed (should_retry=false), not retrying");
        }
    } else if (info->type == ESP_PEER_MSG_TYPE_CANDIDATE) {
        ESP_LOGI(TAG, "Publisher generated ICE candidate (%d bytes)", info->size);
        ESP_LOGI(TAG, "Publisher ICE candidate: %.*s", info->size, (const char *)info->data);
        sfu_send_ice_trickle_http(client, stream_video_sfu_PeerType_PEER_TYPE_PUBLISHER_UNSPECIFIED,
                                  (const char *)info->data);
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

static void pub_peer_loop_task(void *arg)
{
    stream_sfu_client_handle_t client = (stream_sfu_client_handle_t)arg;
    while (client && client->pub_peer) {
        esp_peer_main_loop(client->pub_peer);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    vTaskDelete(NULL);
}

static void publish_task(void *arg)
{
    stream_sfu_client_handle_t client = (stream_sfu_client_handle_t)arg;
    uint32_t video_count = 0;
    uint32_t audio_count = 0;
    uint64_t audio_bitrate_bytes = 0;
    int64_t audio_bitrate_start_us = 0;
    uint32_t audio_bitrate_frames = 0;
    uint32_t audio_bitrate_min = 0;
    uint32_t audio_bitrate_max = 0;
#ifdef CONFIG_STREAM_AUDIO_DUMP_OPUS
    int64_t audio_dump_start_us = 0;
    uint32_t audio_dump_frames = 0;
    size_t audio_dump_bytes = 0;
    bool audio_dump_done = false;
#endif
    ESP_LOGI(TAG, "publish_task started (sink=%p pub_peer=%p video=%d audio=%d)",
             (void *)client->publish_sink,
             (void *)client->pub_peer,
             client->publish_video ? 1 : 0,
             client->publish_audio ? 1 : 0);

    ESP_LOGI(TAG, "publish_task: waiting for publisher_ready (SetPublisher must succeed first)");
    while (client && client->publish_running && !client->publisher_ready) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (!client || !client->publish_running) {
        ESP_LOGW(TAG, "publish_task: aborted while waiting for publisher_ready");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "publish_task: publisher_ready, starting media transmission");

    while (client && client->publish_running && client->publish_sink && client->pub_peer) {
        if (client->publish_video) {
            esp_capture_stream_frame_t frame = {
                .stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO,
            };
            esp_capture_err_t err = esp_capture_sink_acquire_frame(client->publish_sink, &frame, false);
            if (err == ESP_CAPTURE_ERR_OK) {
                esp_peer_video_frame_t vframe = {
                    .pts = frame.pts,
                    .data = frame.data,
                    .size = frame.size,
                };
                esp_peer_send_video(client->pub_peer, &vframe);
                video_count++;
                if (video_count == 1) {
                    ESP_LOGI(TAG, "Sent first video frame (size=%u pts=%u)",
                             (unsigned)frame.size,
                             (unsigned)frame.pts);
                }
                if ((video_count % 60) == 0) {
                    ESP_LOGI(TAG, "Sent video frames: %u (last size=%u pts=%u)",
                             (unsigned)video_count,
                             (unsigned)frame.size,
                             (unsigned)frame.pts);
                }
                esp_capture_sink_release_frame(client->publish_sink, &frame);
            } else {
                static uint32_t video_errs = 0;
                video_errs++;
                if (video_errs <= 5 || (video_errs % 200) == 0) {
                    ESP_LOGW(TAG, "Video frame acquire failed: err=%d (count=%u)",
                             (int)err, (unsigned)video_errs);
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        if (client->publish_audio) {
            bool got_audio = false;
            while (true) {
                esp_capture_stream_frame_t frame = {
                    .stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO,
                };
                esp_capture_err_t err = esp_capture_sink_acquire_frame(client->publish_sink, &frame, true);
                if (err != ESP_CAPTURE_ERR_OK) {
                    break;
                }
                got_audio = true;
                if (audio_bitrate_start_us == 0) {
                    audio_bitrate_start_us = esp_timer_get_time();
                    audio_bitrate_min = (uint32_t)frame.size;
                    audio_bitrate_max = (uint32_t)frame.size;
                }
                audio_bitrate_bytes += frame.size;
                audio_bitrate_frames++;
                if ((uint32_t)frame.size < audio_bitrate_min) {
                    audio_bitrate_min = (uint32_t)frame.size;
                }
                if ((uint32_t)frame.size > audio_bitrate_max) {
                    audio_bitrate_max = (uint32_t)frame.size;
                }
#ifdef CONFIG_STREAM_AUDIO_DUMP_OPUS
                if (!audio_dump_done) {
                    if (!s_opus_dump_buf) {
                        s_opus_dump_cap = (size_t)CONFIG_STREAM_AUDIO_DUMP_MAX_BYTES;
                        s_opus_dump_buf = (uint8_t *)malloc(s_opus_dump_cap);
                        s_opus_dump_len = 0;
                        if (s_opus_dump_buf) {
                            audio_dump_start_us = esp_timer_get_time();
                            ESP_LOGI(TAG, "Opus RAM dump started: cap=%u bytes (%ds) buf=%p",
                                     (unsigned)s_opus_dump_cap,
                                     (int)CONFIG_STREAM_AUDIO_DUMP_SECONDS,
                                     (void *)s_opus_dump_buf);
                        } else {
                            ESP_LOGW(TAG, "Opus RAM dump alloc failed (%u bytes)",
                                     (unsigned)CONFIG_STREAM_AUDIO_DUMP_MAX_BYTES);
                            audio_dump_done = true;
                        }
                    } else if (s_opus_dump_len == 0 && audio_dump_start_us == 0) {
                        audio_dump_start_us = esp_timer_get_time();
                        ESP_LOGI(TAG, "Opus RAM dump restarted: cap=%u bytes buf=%p",
                                 (unsigned)s_opus_dump_cap,
                                 (void *)s_opus_dump_buf);
                    }
                    if (s_opus_dump_buf) {
                        int64_t now_us = esp_timer_get_time();
                        if ((now_us - audio_dump_start_us) <=
                            ((int64_t)CONFIG_STREAM_AUDIO_DUMP_SECONDS * 1000000LL)) {
                            uint32_t frame_len = (uint32_t)frame.size;
                            size_t needed = sizeof(frame_len) + frame.size;
                            if (s_opus_dump_len + needed <= s_opus_dump_cap) {
                                memcpy(s_opus_dump_buf + s_opus_dump_len, &frame_len, sizeof(frame_len));
                                s_opus_dump_len += sizeof(frame_len);
                                memcpy(s_opus_dump_buf + s_opus_dump_len, frame.data, frame.size);
                                s_opus_dump_len += frame.size;
                                audio_dump_frames++;
                                audio_dump_bytes += frame.size;
                            } else {
                                audio_dump_done = true;
                                ESP_LOGW(TAG, "Opus RAM dump full at %u bytes (cap=%u)",
                                         (unsigned)s_opus_dump_len,
                                         (unsigned)s_opus_dump_cap);
                            }
                        } else {
                            audio_dump_done = true;
                            ESP_LOGI(TAG, "Opus RAM dump complete: frames=%u bytes=%u used=%u buf=%p",
                                     (unsigned)audio_dump_frames,
                                     (unsigned)audio_dump_bytes,
                                     (unsigned)s_opus_dump_len,
                                     (void *)s_opus_dump_buf);
                        }
                    }
                }
#endif
                esp_peer_audio_frame_t aframe = {
                    .pts = frame.pts,
                    .data = frame.data,
                    .size = frame.size,
                };
                esp_peer_send_audio(client->pub_peer, &aframe);
                audio_count++;
                if (audio_count == 1) {
                    ESP_LOGI(TAG, "Sent first audio frame (size=%u pts=%u)",
                             (unsigned)frame.size,
                             (unsigned)frame.pts);
                }
                if ((audio_count % 120) == 0) {
                    ESP_LOGI(TAG, "Sent audio frames: %u (last size=%u pts=%u)",
                             (unsigned)audio_count,
                             (unsigned)frame.size,
                             (unsigned)frame.pts);
                }
                if (audio_bitrate_start_us > 0) {
                    int64_t now_us = esp_timer_get_time();
                    if ((now_us - audio_bitrate_start_us) >= 5000000LL) {
                        double seconds = (double)(now_us - audio_bitrate_start_us) / 1000000.0;
                        double kbps = (audio_bitrate_bytes * 8.0) / (seconds * 1000.0);
                        uint32_t avg_size = audio_bitrate_frames ?
                            (uint32_t)(audio_bitrate_bytes / audio_bitrate_frames) : 0;
                        ESP_LOGI(TAG,
                                 "Audio encoded bitrate: %.1f kbps over %.1fs (bytes=%u frames=%u avg=%u min=%u max=%u)",
                                 kbps,
                                 seconds,
                                 (unsigned)audio_bitrate_bytes,
                                 (unsigned)audio_bitrate_frames,
                                 (unsigned)avg_size,
                                 (unsigned)audio_bitrate_min,
                                 (unsigned)audio_bitrate_max);
                        audio_bitrate_bytes = 0;
                        audio_bitrate_frames = 0;
                        audio_bitrate_min = 0;
                        audio_bitrate_max = 0;
                        audio_bitrate_start_us = now_us;
                    }
                }
                esp_capture_sink_release_frame(client->publish_sink, &frame);
            }
            if (!got_audio) {
                static uint32_t audio_errs = 0;
                audio_errs++;
                if (audio_errs <= 5 || (audio_errs % 200) == 0) {
                    ESP_LOGW(TAG, "Audio frame acquire failed: err=%d (count=%u)",
                             (int)ESP_CAPTURE_ERR_NOT_FOUND, (unsigned)audio_errs);
                }
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
    }
#ifdef CONFIG_STREAM_AUDIO_DUMP_OPUS
    if (s_opus_dump_buf && !audio_dump_done) {
        ESP_LOGI(TAG, "Opus RAM dump stopped: frames=%u bytes=%u used=%u buf=%p",
                 (unsigned)audio_dump_frames,
                 (unsigned)audio_dump_bytes,
                 (unsigned)s_opus_dump_len,
                 (void *)s_opus_dump_buf);
    }
#endif
    vTaskDelete(NULL);
}

static stream_video_error_t sfu_peer_ensure(stream_sfu_client_handle_t client)
{
    if (!client || client->peer) {
        return STREAM_VIDEO_ERR_OK;
    }

    ESP_LOGI(TAG, "Creating peer connection (subscriber)");
    esp_peer_default_cfg_t default_cfg = {0};
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
            .channel = STREAM_SFU_AUDIO_CHANNELS,
        },
        .video_info = {
            .codec = ESP_PEER_VIDEO_CODEC_H264,
            .width = 1280,
            .height = 720,
            .fps = 30,
        },
        .enable_data_channel = false,
        .manual_ch_create = false,
        .extra_cfg = &default_cfg,
        .extra_size = sizeof(default_cfg),
        .on_state = peer_state_handler_sub,
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
    xTaskCreate(peer_loop_task, "sfu_peer", SFU_PEER_TASK_STACK_BYTES, client, 5, &client->peer_task);
    return STREAM_VIDEO_ERR_OK;
}

static stream_video_error_t sfu_pub_peer_ensure(stream_sfu_client_handle_t client)
{
    if (!client || client->pub_peer) {
        return STREAM_VIDEO_ERR_OK;
    }

    ESP_LOGI(TAG, "Creating publisher peer connection");
    esp_peer_default_cfg_t default_cfg = {0};
    esp_peer_cfg_t cfg = {
        .server_lists = client->ice_servers,
        .server_num = (uint8_t)client->ice_server_count,
        .role = ESP_PEER_ROLE_CONTROLLING,
        .ice_trans_policy = ESP_PEER_ICE_TRANS_POLICY_ALL,
        .audio_dir = ESP_PEER_MEDIA_DIR_SEND_ONLY,
        .video_dir = ESP_PEER_MEDIA_DIR_SEND_ONLY,
        .audio_info = {
            .codec = ESP_PEER_AUDIO_CODEC_OPUS,
            .sample_rate = 48000,
            .channel = STREAM_SFU_AUDIO_CHANNELS,
        },
        .video_info = {
            .codec = ESP_PEER_VIDEO_CODEC_H264,
            .width = 160,
            .height = 120,
            .fps = 10,
        },
        .enable_data_channel = false,
        .manual_ch_create = false,
        .extra_cfg = &default_cfg,
        .extra_size = sizeof(default_cfg),
        .on_state = peer_state_handler_pub,
        .on_msg = peer_msg_handler_pub,
        .ctx = client,
    };

    if (esp_peer_open(&cfg, esp_peer_get_default_impl(), &client->pub_peer) != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to create publisher peer");
        client->pub_peer = NULL;
        return STREAM_VIDEO_ERR_FAIL;
    }

    if (esp_peer_new_connection(client->pub_peer) != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to start publisher peer");
        esp_peer_close(client->pub_peer);
        client->pub_peer = NULL;
        return STREAM_VIDEO_ERR_FAIL;
    }

    ESP_LOGI(TAG, "Publisher peer started");
    xTaskCreate(pub_peer_loop_task, "sfu_pub", SFU_PUB_TASK_STACK_BYTES, client, 5, &client->pub_task);
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

    static const char *subscriber_sdp =
        "v=0\r\n"
        "o=- 0 0 IN IP4 0.0.0.0\r\n"
        "s=-\r\n"
        "t=0 0\r\n"
        "a=group:BUNDLE 0 1\r\n"
        "a=msid-semantic: WMS\r\n"
        "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "a=mid:0\r\n"
        "a=recvonly\r\n"
        "a=rtcp-mux\r\n"
        "a=rtpmap:111 opus/48000/2\r\n"
        "a=fmtp:111 minptime=10;useinbandfec=1\r\n"
        "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "a=mid:1\r\n"
        "a=recvonly\r\n"
        "a=rtcp-mux\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\n"
        "a=rtcp-fb:96 nack\r\n"
        "a=rtcp-fb:96 nack pli\r\n"
        "a=rtcp-fb:96 transport-cc\r\n"
        "a=rtcp-fb:96 ccm fir\r\n"
        "a=rtcp-fb:96 goog-remb\r\n";
    join_req.subscriber_sdp.funcs.encode = encode_string_cb;
    join_req.subscriber_sdp.arg = (void *)subscriber_sdp;

    join_req.has_client_details = true;
    join_req.client_details.has_sdk = true;
    join_req.client_details.sdk.type = stream_video_sfu_SdkType_SDK_TYPE_UNSPECIFIED;
    static const char *sdk_major = "0";
    static const char *sdk_minor = "1";
    static const char *sdk_patch = "0";
    join_req.client_details.sdk.major.funcs.encode = encode_string_cb;
    join_req.client_details.sdk.major.arg = (void *)sdk_major;
    join_req.client_details.sdk.minor.funcs.encode = encode_string_cb;
    join_req.client_details.sdk.minor.arg = (void *)sdk_minor;
    join_req.client_details.sdk.patch.funcs.encode = encode_string_cb;
    join_req.client_details.sdk.patch.arg = (void *)sdk_patch;

    join_req.client_details.has_os = true;
    static const char *os_name = "esp-idf";
    static const char *os_arch = "xtensa";
    join_req.client_details.os.name.funcs.encode = encode_string_cb;
    join_req.client_details.os.name.arg = (void *)os_name;
    join_req.client_details.os.architecture.funcs.encode = encode_string_cb;
    join_req.client_details.os.architecture.arg = (void *)os_arch;

    join_req.client_details.has_device = true;
    static const char *dev_name = "esp32s3";
    join_req.client_details.device.name.funcs.encode = encode_string_cb;
    join_req.client_details.device.name.arg = (void *)dev_name;

    if (client->has_joined) {
        client->reconnect_attempt++;
        join_req.fast_reconnect = true;
        join_req.has_reconnect_details = true;
        join_req.reconnect_details.strategy =
            stream_video_sfu_WebsocketReconnectStrategy_WEBSOCKET_RECONNECT_STRATEGY_FAST;
        join_req.reconnect_details.reconnect_attempt = client->reconnect_attempt;
        join_req.reconnect_details.previous_session_id.funcs.encode = encode_string_cb;
        join_req.reconnect_details.previous_session_id.arg = (void *)client->join_session_id;
        ESP_LOGI(TAG, "SFU join request (FAST reconnect #%lu): token_len=%u session_id=%s",
                 (unsigned long)client->reconnect_attempt,
                 (unsigned)strlen(client->join_token),
                 client->join_session_id);
    } else {
        ESP_LOGI(TAG, "SFU join request (first join): token_len=%u session_id=%s",
                 (unsigned)strlen(client->join_token),
                 client->join_session_id);
    }

    request.request_payload.join_request = join_req;

    size_t buffer_size = 2048;
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

    ESP_LOGI(TAG, "SFU join request encoded: %u bytes", (unsigned)stream.bytes_written);
    if (stream.bytes_written >= 4) {
        ESP_LOGI(TAG, "SFU join request first bytes: %02x %02x %02x %02x",
                 buffer[0], buffer[1], buffer[2], buffer[3]);
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
    if (len != (int)stream.bytes_written) {
        ESP_LOGW(TAG, "SFU join request: sent %d of %u bytes",
                 len, (unsigned)stream.bytes_written);
    }

    ESP_LOGI(TAG, "✓ SFU join request sent (%d bytes)", len);
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
            client->reconnect_attempts = 0;
            client->last_event_tick = xTaskGetTickCount();
            client->ws_connected_tick = xTaskGetTickCount();
            xEventGroupClearBits(client->event_group, SFU_DISCONNECTED_BIT | SFU_ERROR_BIT);
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
                    ESP_LOGW(TAG, "Failed to send SFU join request on connect: %d", err);
                }
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED: {
            TickType_t now = xTaskGetTickCount();
            uint32_t ws_uptime_ms = 0;
            if (client->ws_connected_tick) {
                ws_uptime_ms = (uint32_t)((now - client->ws_connected_tick) * portTICK_PERIOD_MS);
            }
            uint32_t since_last_event_ms = (uint32_t)((now - client->last_event_tick) * portTICK_PERIOD_MS);
            ESP_LOGW(TAG, "SFU WebSocket DISCONNECTED — uptime=%lu ms, last_event=%lu ms ago, "
                     "state=%d, publishing=%d, session=%s",
                     (unsigned long)ws_uptime_ms,
                     (unsigned long)since_last_event_ms,
                     (int)client->state,
                     (int)client->publish_running,
                     client->join_session_id[0] ? client->join_session_id : "(none)");
            client->ws_connected_tick = 0;
            client->state = STREAM_SFU_STATE_DISCONNECTED;
            xEventGroupSetBits(client->event_group, SFU_DISCONNECTED_BIT);

            if (client->config.event_cb) {
                stream_sfu_event_t event = {
                    .type = STREAM_SFU_EVENT_DISCONNECTED,
                    .data = NULL,
                    .data_len = 0
                };
                client->config.event_cb(client, &event, client->config.user_data);
            }
            break;
        }

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

                stream_video_sfu_SfuEvent event_probe = stream_video_sfu_SfuEvent_init_zero;
                pb_istream_t stream = pb_istream_from_buffer(
                    (const uint8_t *)data->data_ptr,
                    (size_t)data->data_len);
                if (!pb_decode(&stream, stream_video_sfu_SfuEvent_fields, &event_probe)) {
                    ESP_LOGE(TAG, "Failed to decode SFU event: %s (len=%d)",
                             PB_GET_ERROR(&stream), data->data_len);
                    break;
                }

                sfu_publish_options_ctx_t publish_ctx = {
                    .options = client->publish_options,
                    .capacity = SFU_MAX_PUBLISH_TRACK_IDS,
                    .count = 0,
                };
                stream_video_sfu_SfuEvent event = stream_video_sfu_SfuEvent_init_zero;
                event.which_event_payload = event_probe.which_event_payload;
                switch (event_probe.which_event_payload) {
                    case stream_video_sfu_SfuEvent_subscriber_offer_tag:
                        event.event_payload.subscriber_offer.sdp.funcs.decode = decode_string_cb;
                        event.event_payload.subscriber_offer.sdp.arg = &sdp_ctx;
                        break;
                    case stream_video_sfu_SfuEvent_publisher_answer_tag:
                        event.event_payload.publisher_answer.sdp.funcs.decode = decode_string_cb;
                        event.event_payload.publisher_answer.sdp.arg = &sdp_ctx;
                        break;
                    case stream_video_sfu_SfuEvent_ice_trickle_tag:
                        event.event_payload.ice_trickle.ice_candidate.funcs.decode = decode_string_cb;
                        event.event_payload.ice_trickle.ice_candidate.arg = &ice_ctx;
                        break;
                    case stream_video_sfu_SfuEvent_error_tag:
                        event.event_payload.error.error.message.funcs.decode = decode_string_cb;
                        event.event_payload.error.error.message.arg = &err_ctx;
                        break;
                    case stream_video_sfu_SfuEvent_join_response_tag:
                        event.event_payload.join_response.publish_options.funcs.decode = decode_publish_options_cb;
                        event.event_payload.join_response.publish_options.arg = &publish_ctx;
                        break;
                    case stream_video_sfu_SfuEvent_change_publish_options_tag:
                        event.event_payload.change_publish_options.publish_options.funcs.decode = decode_publish_options_cb;
                        event.event_payload.change_publish_options.publish_options.arg = &publish_ctx;
                        break;
                    default:
                        break;
                }

                stream = pb_istream_from_buffer(
                    (const uint8_t *)data->data_ptr,
                    (size_t)data->data_len);
                if (pb_decode_ex(&stream, stream_video_sfu_SfuEvent_fields, &event,
                                 PB_DECODE_NOINIT)) {
                    client->publish_option_count = publish_ctx.count;
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
                            if (ice_buf[0]) {
                                char parsed_candidate[256];
                                const char *candidate = ice_buf;
                                if (ice_buf[0] == '{') {
                                    if (sfu_extract_candidate_from_json(
                                            ice_buf,
                                            parsed_candidate,
                                            sizeof(parsed_candidate))) {
                                        candidate = parsed_candidate;
                                    } else {
                                        ESP_LOGW(TAG, "Failed to parse ICE candidate JSON");
                                    }
                                }
                                esp_peer_handle_t target = NULL;
                                if (event.event_payload.ice_trickle.peer_type ==
                                    stream_video_sfu_PeerType_PEER_TYPE_SUBSCRIBER) {
                                    target = client->peer;
                                } else {
                                    target = client->pub_peer;
                                }
                                if (target) {
                                    esp_peer_msg_t msg = {
                                        .type = ESP_PEER_MSG_TYPE_CANDIDATE,
                                        .data = (uint8_t *)candidate,
                                        .size = (int)strlen(candidate),
                                    };
                                    esp_peer_send_msg(target, &msg);
                                }
                            } else {
                                ESP_LOGW(TAG, "SFU ICE trickle has empty candidate (peer=%d)",
                                         (int)event.event_payload.ice_trickle.peer_type);
                            }
                            break;
                        case stream_video_sfu_SfuEvent_join_response_tag: {
                            bool was_reconnected = event.event_payload.join_response.reconnected;
                            ESP_LOGI(TAG, "SFU join response received");
                            ESP_LOGI(TAG, "Join response: reconnected=%d deadline=%d publish_options=%u",
                                     (int)was_reconnected,
                                     (int)event.event_payload.join_response.fast_reconnect_deadline_seconds,
                                     (unsigned)client->publish_option_count);
                            if (client->publish_option_count) {
                                ESP_LOGI(TAG, "SFU publish options: %u", (unsigned)client->publish_option_count);
                                for (size_t i = 0; i < client->publish_option_count; ++i) {
                                    ESP_LOGI(TAG, "Publish option[%u]: id=%d type=%d codec=%s %ux%u@%u",
                                             (unsigned)i,
                                             (int)client->publish_options[i].id,
                                             (int)client->publish_options[i].track_type,
                                             client->publish_options[i].codec_name[0] ? client->publish_options[i].codec_name : "(none)",
                                             (unsigned)client->publish_options[i].width,
                                             (unsigned)client->publish_options[i].height,
                                             (unsigned)client->publish_options[i].fps);
                                }
                            }
                            if (!was_reconnected && (client->pub_peer || client->publish_task)) {
                                ESP_LOGW(TAG, "Fresh join (reconnected=0): tearing down stale publisher "
                                         "(matching Android SDK rejoin behaviour)");
                                client->publish_running = false;
                                client->publisher_ready = false;
                                if (client->publish_task) {
                                    vTaskDelete(client->publish_task);
                                    client->publish_task = NULL;
                                }
                                if (client->pub_peer) {
                                    esp_peer_handle_t old_pub = client->pub_peer;
                                    TaskHandle_t old_task = client->pub_task;
                                    client->pub_peer = NULL;
                                    client->pub_task = NULL;
                                    if (old_task) {
                                        vTaskDelete(old_task);
                                    }
                                    esp_peer_close(old_pub);
                                    vTaskDelay(pdMS_TO_TICKS(100));
                                } else if (client->pub_task) {
                                    vTaskDelete(client->pub_task);
                                    client->pub_task = NULL;
                                }
                                sfu_reset_publish_track_ids(client);
                            }
                            client->has_joined = true;
                            if (was_reconnected) {
                                ESP_LOGI(TAG, "SFU confirmed fast reconnect — session preserved");
                                client->reconnect_attempt = 0;
                            }
                            if (client->config.on_join_response) {
                                client->config.on_join_response(client, client->config.user_data);
                            }
                            break;
                        }
                        case stream_video_sfu_SfuEvent_change_publish_options_tag:
                            ESP_LOGI(TAG, "SFU change publish options received");
                            if (client->publish_option_count) {
                                ESP_LOGI(TAG, "SFU publish options: %u", (unsigned)client->publish_option_count);
                                for (size_t i = 0; i < client->publish_option_count; ++i) {
                                    ESP_LOGI(TAG, "Publish option[%u]: id=%d type=%d codec=%s %ux%u@%u",
                                             (unsigned)i,
                                             (int)client->publish_options[i].id,
                                             (int)client->publish_options[i].track_type,
                                             client->publish_options[i].codec_name[0] ? client->publish_options[i].codec_name : "(none)",
                                             (unsigned)client->publish_options[i].width,
                                             (unsigned)client->publish_options[i].height,
                                             (unsigned)client->publish_options[i].fps);
                                }
                            }
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

        case WEBSOCKET_EVENT_ERROR: {
            TickType_t now_err = xTaskGetTickCount();
            uint32_t err_uptime_ms = 0;
            if (client->ws_connected_tick) {
                err_uptime_ms = (uint32_t)((now_err - client->ws_connected_tick) * portTICK_PERIOD_MS);
            }
            ESP_LOGE(TAG, "SFU WebSocket ERROR — uptime=%lu ms, state=%d, publishing=%d, session=%s",
                     (unsigned long)err_uptime_ms,
                     (int)client->state,
                     (int)client->publish_running,
                     client->join_session_id[0] ? client->join_session_id : "(none)");
            if (data) {
                ESP_LOGE(TAG, "SFU WS error detail: type=%d, tls_err=0x%x, tls_stack=%d, "
                         "sock_errno=%d, handshake_status=%d",
                         (int)data->error_handle.error_type,
                         data->error_handle.esp_tls_last_esp_err,
                         data->error_handle.esp_tls_stack_err,
                         data->error_handle.esp_transport_sock_errno,
                         data->error_handle.esp_ws_handshake_status_code);
            }
            client->state = STREAM_SFU_STATE_ERROR;
            xEventGroupSetBits(client->event_group, SFU_ERROR_BIT);

            if (client->config.event_cb) {
                stream_sfu_event_t event = {
                    .type = STREAM_SFU_EVENT_ERROR,
                    .data = NULL,
                    .data_len = 0
                };
                client->config.event_cb(client, &event, client->config.user_data);
            }
            break;
        }

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
    sfu_reset_publish_options(client);

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

    esp_websocket_client_config_t ws_cfg = {
        .uri = config->ws_endpoint,
        .headers = client->ws_headers,
        .reconnect_timeout_ms = 10000,
        .network_timeout_ms = 10000,
        .task_stack = 8192,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_pingpong_discon = true,
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
    client->reconnect_task = NULL;
    client->should_reconnect = true;
    client->reconnect_attempts = 0;
    if (client->config.reconnect_interval_ms == 0) {
        client->config.reconnect_interval_ms = 5000;
    }
    client->last_event_tick = xTaskGetTickCount();
    client->join_token[0] = '\0';
    client->http_auth_header[0] = '\0';
    client->join_session_id[0] = '\0';
    client->http_url[0] = '\0';
    client->ws_headers[0] = '\0';
    client->publisher_ready = false;
    client->peer = NULL;
    client->peer_task = NULL;
    client->pub_peer = NULL;
    client->pub_task = NULL;
    client->publish_task = NULL;
    client->publish_sink = NULL;
    client->publish_audio = false;
    client->publish_video = false;
    client->publish_running = false;
    client->ice_servers = NULL;
    client->ice_server_count = 0;
    xTaskCreate(sfu_health_monitor_task, "sfu_health", 4096, client, 5, &client->health_task);
    xTaskCreate(sfu_reconnect_task, "sfu_reconn", 4096, client, 5, &client->reconnect_task);

    *client_out = client;
    return STREAM_VIDEO_ERR_OK;
}

void stream_sfu_client_destroy(stream_sfu_client_handle_t client)
{
    if (!client) {
        return;
    }

    client->should_reconnect = false;

    // Disconnect if connected
    if (client->ws_client) {
        esp_websocket_client_stop(client->ws_client);
        esp_websocket_client_destroy(client->ws_client);
    }

    if (client->reconnect_task) {
        vTaskDelete(client->reconnect_task);
        client->reconnect_task = NULL;
    }
    if (client->health_task) {
        vTaskDelete(client->health_task);
    }

    // Clean up event group (after tasks that wait on it are gone)
    if (client->event_group) {
        vEventGroupDelete(client->event_group);
    }

    if (client->peer) {
        esp_peer_close(client->peer);
        client->peer = NULL;
    }
    if (client->peer_task) {
        vTaskDelete(client->peer_task);
    }
    if (client->publish_task) {
        vTaskDelete(client->publish_task);
    }
    if (client->pub_peer) {
        esp_peer_close(client->pub_peer);
        client->pub_peer = NULL;
    }
    if (client->pub_task) {
        vTaskDelete(client->pub_task);
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

    client->should_reconnect = false;
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
    snprintf(client->http_auth_header, sizeof(client->http_auth_header),
             "Bearer %s", token);
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

stream_video_error_t stream_sfu_client_start_publishing(
    stream_sfu_client_handle_t client,
    esp_capture_sink_handle_t sink,
    bool publish_audio,
    bool publish_video)
{
    if (!client || !sink) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }
    if (client->state != STREAM_SFU_STATE_CONNECTED) {
        return STREAM_VIDEO_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "start_publishing: sink=%p audio=%d video=%d state=%d",
             (void *)sink,
             publish_audio ? 1 : 0,
             publish_video ? 1 : 0,
             (int)client->state);

    stream_video_error_t err = sfu_pub_peer_ensure(client);
    if (err != STREAM_VIDEO_ERR_OK) {
        return err;
    }

    client->publish_sink = sink;
    client->publish_audio = publish_audio;
    client->publish_video = publish_video;
    client->publish_running = true;
    sfu_reset_publish_track_ids(client);
    sfu_reset_publish_options(client);

    if (!client->publish_task) {
        BaseType_t task_ok = xTaskCreate(publish_task, "sfu_publish", 8192, client, 5,
                                         &client->publish_task);
        ESP_LOGI(TAG, "start_publishing: publish_task created=%d handle=%p",
                 (int)task_ok,
                 (void *)client->publish_task);
    } else {
        ESP_LOGI(TAG, "start_publishing: publish_task already running handle=%p",
                 (void *)client->publish_task);
    }

    return STREAM_VIDEO_ERR_OK;
}

stream_video_error_t stream_sfu_client_stop_publishing(stream_sfu_client_handle_t client)
{
    if (!client) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }
    client->publish_running = false;
    client->publish_sink = NULL;
    sfu_reset_publish_track_ids(client);
    sfu_reset_publish_options(client);
    if (client->publish_task) {
        vTaskDelete(client->publish_task);
        client->publish_task = NULL;
    }
    if (client->pub_peer) {
        esp_peer_close(client->pub_peer);
        client->pub_peer = NULL;
    }
    if (client->pub_task) {
        vTaskDelete(client->pub_task);
        client->pub_task = NULL;
    }
    return STREAM_VIDEO_ERR_OK;
}

stream_sfu_state_t stream_sfu_client_get_state(stream_sfu_client_handle_t client)
{
    if (!client) {
        return STREAM_SFU_STATE_DISCONNECTED;
    }
    return client->state;
}