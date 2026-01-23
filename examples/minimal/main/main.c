/**
 * @file main.c
 * @brief Example using SDK join/leave APIs
 *
 * The app layer only calls SDK join/leave.
 * The SDK handles auth, coordinator connect, joinCall, and SFU connect.
 *
 * Configuration:
 * - User: Selected via STREAM_USER_ID
 * - Environment: Selected via STREAM_ENVIRONMENT
 * - Call ID: Specified via STREAM_CALL_ID
 * - Call Type: Specified via STREAM_CALL_TYPE
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
#include "stream_video.h"
#include "stream_video_token.h"

static const char *TAG = "main";

// ============================================================================
// CONFIGURATION - Edit these values for your setup
// ============================================================================

// WiFi credentials - configure these for your network
#define WIFI_SSID "Airtel_s3"
#define WIFI_PASSWORD "J@rd!ns3"

// User and Environment Selection (for auth request)
#define STREAM_ENVIRONMENT "pronto"  // "production", "staging", or "development"
#define STREAM_USER_ID "esp32_user"         // User ID to authenticate as (can be NULL for auto-generated)

// Call Selection (for joining call)
#define STREAM_CALL_TYPE "default"       // Call type (e.g., "default", "livestream")
#define STREAM_CALL_ID "79cYh3J5JgGk"         // Call ID to join (can be NULL to create new call)

// Global state
static stream_video_client_handle_t g_client = NULL;
static bool g_wifi_connected = false;
static bool g_flow_started = false;
static TaskHandle_t g_flow_task = NULL;
static bool g_join_complete = false;
static bool g_join_success = false;

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

    sync_time();

    stream_video_join_call_params_t params = {
        .environment = STREAM_ENVIRONMENT,
        .user_id = STREAM_USER_ID,
        .exp = STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS,
        .call_type = STREAM_CALL_TYPE,
        .call_id = STREAM_CALL_ID,
        .create = true,
        .location = NULL,
        .result_cb = on_join_result,
        .user_data = NULL,
    };

    stream_video_error_t err = stream_video_join_call(&params, &g_client);

    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to start join flow: %d", err);
    }

    g_flow_task = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief WiFi event handler
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
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
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Stream Video ESP32-S3 - Complete Flow");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  Environment: %s", STREAM_ENVIRONMENT);
    ESP_LOGI(TAG, "  User ID: %s", STREAM_USER_ID ? STREAM_USER_ID : "NULL (auto-generated)");
    ESP_LOGI(TAG, "  Call Type: %s", STREAM_CALL_TYPE);
    ESP_LOGI(TAG, "  Call ID: %s", STREAM_CALL_ID ? STREAM_CALL_ID : "NULL (create new)");
    ESP_LOGI(TAG, "========================================");

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
