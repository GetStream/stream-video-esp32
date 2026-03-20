/**
 * @file main.c
 * @brief Example using SDK join/leave APIs
 *
 * The app fetches the token from its token service (or Stream's), then passes
 * auth data to the SDK. The SDK handles coordinator connect, joinCall, and SFU connect.
 *
 * Configuration (edit for your setup):
 * - WiFi: sdkconfig.defaults or idf.py menuconfig (Stream Video Example)
 * - Token service: STREAM_AUTH_BASE_URL, STREAM_ENVIRONMENT, STREAM_USER_ID
 * - Call: STREAM_CALL_TYPE, STREAM_CALL_ID below
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include "stream_video.h"
#include "stream_video_auth.h"
#include "stream_video_capture.h"
#include "sdkconfig.h"
#include "stream_video_token.h"
#include "app_token.h"

static const char *TAG = "main";

// ============================================================================
// CONFIGURATION - Edit these values for your setup
// ============================================================================

// WiFi credentials (set in menuconfig)
#define WIFI_SSID CONFIG_STREAM_VIDEO_WIFI_SSID
#define WIFI_PASSWORD CONFIG_STREAM_VIDEO_WIFI_PASSWORD

// Token service and user (app fetches token, then passes to SDK)
#define STREAM_AUTH_BASE_URL "https://pronto.getstream.io/"  // Your backend or Stream's token service
#define STREAM_ENVIRONMENT "pronto"   // "production", "staging", or "pronto"
#define STREAM_USER_ID "esp32_user"   // User ID (can be NULL for auto-generated)

// Call Selection (for joining call) - edit for your call or set to NULL to create new
#define STREAM_CALL_TYPE "default"    // Call type (e.g., "default", "livestream")
#define STREAM_CALL_ID "79cYh3J5JgGk" // Call ID to join (can be NULL to create new call)

// Global state
static stream_video_client_handle_t g_client = NULL;
static bool g_wifi_connected = false;
static bool g_flow_started = false;
static TaskHandle_t g_flow_task = NULL;
static bool g_join_complete = false;
static bool g_join_success = false;

static void stun_udp_probe(void)
{
    const char *host = "stun.l.google.com";
    const char *port = "19302";
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
    };
    struct addrinfo *res = NULL;
    int err = getaddrinfo(host, port, &hints, &res);
    if (err != 0 || !res) {
        ESP_LOGE(TAG, "STUN probe: DNS failed (%d)", err);
        return;
    }

    char ip_str[INET_ADDRSTRLEN] = {0};
    struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
    inet_ntop(AF_INET, &addr->sin_addr, ip_str, sizeof(ip_str));
    ESP_LOGI(TAG, "STUN probe: %s:%s -> %s", host, port, ip_str);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "STUN probe: socket failed");
        freeaddrinfo(res);
        return;
    }

    struct timeval tv = {
        .tv_sec = 2,
        .tv_usec = 0,
    };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t req[20] = {
        0x00, 0x01, 0x00, 0x00, // Binding Request, length 0
        0x21, 0x12, 0xA4, 0x42, // Magic cookie
        0x12, 0x34, 0x56, 0x78, // Transaction ID (12 bytes)
        0x9A, 0xBC, 0xDE, 0xF0,
        0x11, 0x22, 0x33, 0x44
    };
    int sent = sendto(sock, req, sizeof(req), 0, res->ai_addr, res->ai_addrlen);
    if (sent < 0) {
        ESP_LOGE(TAG, "STUN probe: send failed");
        close(sock);
        freeaddrinfo(res);
        return;
    }

    uint8_t resp[128] = {0};
    int recv_len = recvfrom(sock, resp, sizeof(resp), 0, NULL, NULL);
    if (recv_len < 0) {
        ESP_LOGE(TAG, "STUN probe: no response");
    } else {
        ESP_LOGI(TAG, "STUN probe: response %d bytes", recv_len);
        if (recv_len >= 20) {
            uint16_t msg_type = (uint16_t)((resp[0] << 8) | resp[1]);
            uint16_t msg_len = (uint16_t)((resp[2] << 8) | resp[3]);
            uint32_t cookie = (uint32_t)((resp[4] << 24) | (resp[5] << 16) | (resp[6] << 8) | resp[7]);
            if (msg_type == 0x0101 && cookie == 0x2112A442 && recv_len >= (20 + msg_len)) {
                int offset = 20;
                while (offset + 4 <= recv_len) {
                    uint16_t attr_type = (uint16_t)((resp[offset] << 8) | resp[offset + 1]);
                    uint16_t attr_len = (uint16_t)((resp[offset + 2] << 8) | resp[offset + 3]);
                    int value_offset = offset + 4;
                    if (value_offset + attr_len > recv_len) {
                        break;
                    }
                    if (attr_type == 0x0020 || attr_type == 0x0001) { // XOR-MAPPED-ADDRESS or MAPPED-ADDRESS
                        if (attr_len >= 8) {
                            uint8_t family = resp[value_offset + 1];
                            uint16_t port = (uint16_t)((resp[value_offset + 2] << 8) | resp[value_offset + 3]);
                            uint32_t addr = (uint32_t)((resp[value_offset + 4] << 24) | (resp[value_offset + 5] << 16) |
                                                       (resp[value_offset + 6] << 8) | resp[value_offset + 7]);
                            if (attr_type == 0x0020) {
                                port ^= (uint16_t)(cookie >> 16);
                                addr ^= cookie;
                            }
                            if (family == 0x01) { // IPv4
                                ip4_addr_t ip4;
                                ip4.addr = htonl(addr);
                                ESP_LOGI(TAG, "STUN probe: mapped address %s:%u",
                                         ip4addr_ntoa(&ip4), (unsigned)port);
                                break;
                            }
                        }
                    }
                    offset = value_offset + attr_len;
                    if (attr_len % 4 != 0) {
                        offset += (4 - (attr_len % 4));
                    }
                }
            }
        }
    }

    close(sock);
    freeaddrinfo(res);
}

static void on_join_result(const stream_video_join_result_t *result, void *user_data)
{
    (void)user_data;
    if (!result) {
        return;
    }

    g_join_complete = true;
    g_join_success = result->success;

    if (result->success) {
        ESP_LOGI(TAG, "✓ Join result: success");
    } else {
        ESP_LOGE(TAG, "✗ Join result: %s",
                 result->error_message[0] ? result->error_message : "unknown error");
        /* Clean up client on join failure so SDK can free resources */
        if (g_client) {
            stream_video_leave_call(g_client);
            g_client = NULL;
        }
    }
}

// Forward declarations
static void stream_flow_task(void *arg);
static esp_err_t sync_time(void);

/**
 * @brief Sync system time via SNTP (needed for TLS)
 */
static esp_err_t sync_time(void)
{
    time_t now = 0;
    struct tm timeinfo = {0};
    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year >= (2020 - 1900)) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Syncing time via SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.cloudflare.com");
    esp_sntp_init();

    for (int retry = 0; retry < 20; ++retry) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year >= (2020 - 1900)) {
            esp_sntp_stop();
            ESP_LOGI(TAG, "Time synced");
            return ESP_OK;
        }
    }

    esp_sntp_stop();
    ESP_LOGW(TAG, "Time sync failed");
    return ESP_FAIL;
}

/**
 * @brief Task to start auth/coordinator flow outside sys_evt
 */
static void stream_flow_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "WiFi connected, starting Stream Video flow...");

    bool psram_ok = esp_psram_is_initialized();
    if (psram_ok) {
        size_t psram_size = esp_psram_get_size();
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        ESP_LOGI(TAG, "PSRAM initialized: %u bytes (free %u)",
                 (unsigned)psram_size, (unsigned)psram_free);
    } else {
        ESP_LOGW(TAG, "PSRAM not initialized");
    }

    stun_udp_probe();
    sync_time();

    /* App fetches token from token service (its own backend or Stream's) */
    stream_video_auth_data_t auth_data = {0};
    stream_video_error_t err = app_request_auth_data(
        STREAM_AUTH_BASE_URL,
        STREAM_ENVIRONMENT,
        STREAM_USER_ID,
        STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS,
        &auth_data);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to get token: %d", err);
        g_flow_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    stream_video_join_call_params_t params = {
        .auth_data = &auth_data,
        .call_type = STREAM_CALL_TYPE,
        .call_id = STREAM_CALL_ID,
        .create = true,
        .location = NULL,
        .result_cb = on_join_result,
        .user_data = NULL,
        .sink = NULL,
        .mute_audio = false,
        .mute_video = false,
    };

    err = stream_video_join_call(&params, &g_client);

    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to start join flow: %d", err);
    }

    g_flow_task = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief WiFi event handler — reconnects immediately on disconnect
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        g_wifi_connected = false;
        ESP_LOGI(TAG, "WiFi disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        g_wifi_connected = true;

        if (!g_flow_started) {
            g_flow_started = true;
            BaseType_t ok = xTaskCreate(
                stream_flow_task,
                "stream_flow",
                6144,
                NULL,
                5,
                &g_flow_task
            );
            if (ok != pdPASS) {
                ESP_LOGE(TAG, "Failed to start stream flow task");
                g_flow_started = false;
            }
        }
    }
}

/**
 * @brief Initialize WiFi
 */
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialized, connecting to %s...", WIFI_SSID);
}


void app_main(void)
{
    esp_log_level_set("AGENT", ESP_LOG_VERBOSE);
    esp_log_level_set("ICE", ESP_LOG_VERBOSE);
    esp_log_level_set("STUN", ESP_LOG_VERBOSE);
    esp_log_level_set("TURN", ESP_LOG_VERBOSE);
    esp_log_level_set("stream_signaling_ws", ESP_LOG_WARN);
    ESP_LOGI(TAG, "Build marker: %s %s", __DATE__, __TIME__);
    ESP_LOGI(TAG, "Build call ID: %s", STREAM_CALL_ID ? STREAM_CALL_ID : "NULL");
    ESP_LOGI(TAG, "Encoder stack bytes: %d", CONFIG_STREAM_VIDEO_VENC_TASK_STACK);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Stream Video ESP32-S3 - Complete Flow");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  Environment: %s", STREAM_ENVIRONMENT);
    ESP_LOGI(TAG, "  User ID: %s", STREAM_USER_ID ? STREAM_USER_ID : "NULL (auto-generated)");
    ESP_LOGI(TAG, "  Call Type: %s", STREAM_CALL_TYPE);
    ESP_LOGI(TAG, "  Call ID: %s", STREAM_CALL_ID ? STREAM_CALL_ID : "NULL (create new)");
#ifdef CONFIG_STREAM_VIDEO_STUN_ONLY
    ESP_LOGI(TAG, "  STUN-only: enabled");
#else
    ESP_LOGI(TAG, "  STUN-only: disabled");
#endif
    ESP_LOGI(TAG, "========================================");

#if defined(CONFIG_STREAM_VIDEO_RUN_RES_TEST)
    ESP_LOGI(TAG, "Running resolution test; streaming is disabled.");
    stream_video_default_capture_run_resolution_test();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

    // Initialize NVS (needed for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "✓ NVS initialized");

    // Initialize Stream Video SDK
    stream_video_error_t err = stream_video_init();
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to initialize Stream Video SDK: %d", err);
        return;
    }
    ESP_LOGI(TAG, "✓ Stream Video SDK initialized");

    // Use SDK default capture (ESP32-S3/P4); SDK will prepare/stop when needed
    stream_video_set_capture_provider(stream_video_default_capture_prepare,
                                      stream_video_default_capture_stop, NULL);

    // Initialize WiFi (this will trigger auth request when WiFi is ready)
    wifi_init();

    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    ESP_LOGI(TAG, "Flow will start automatically when WiFi is connected");

    // Keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        if (g_wifi_connected) {
            const char *join_state = "NOT_STARTED";
            if (g_client && !g_join_complete) {
                join_state = "IN_PROGRESS";
            } else if (g_join_complete) {
                join_state = g_join_success ? "SUCCESS" : "FAILED";
            }
            ESP_LOGI(TAG, "Status: WiFi=OK, JoinFlow=%s", join_state);
        } else {
            ESP_LOGI(TAG, "Status: WiFi=Connecting...");
        }
    }
}
