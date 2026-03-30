#include "capture_internal.h"
#include "stream_video.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include <inttypes.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_capture.h"
#include "esp_capture_sink.h"
#include "esp_capture_advance.h"
#include "esp_capture_video_dvp_src.h"
#include "sdkconfig.h"
#ifdef __has_include
#if __has_include("esp_capture_audio_dev_src.h") && __has_include("esp_codec_dev.h")
#include "esp_capture_audio_dev_src.h"
#define HAVE_CAPTURE_AUDIO_DEV 1
#else
#define HAVE_CAPTURE_AUDIO_DEV 0
#endif
#else
#define HAVE_CAPTURE_AUDIO_DEV 0
#endif
#ifdef __has_include
#if __has_include("encoder/esp_audio_enc_default.h")
#include "encoder/esp_audio_enc_default.h"
#define HAVE_AUDIO_ENC_DEFAULT 1
#else
#define HAVE_AUDIO_ENC_DEFAULT 0
#endif
#if __has_include("esp_gmf_audio_enc.h")
#include "esp_gmf_audio_enc.h"
#endif
#if __has_include("esp_gmf_alc.h")
#include "esp_gmf_alc.h"
#define HAVE_GMF_ALC 1
#else
#define HAVE_GMF_ALC 0
#endif
#if __has_include("esp_gmf_audio_param.h")
#include "esp_gmf_audio_param.h"
#define HAVE_GMF_AUDIO_PARAM 1
#else
#define HAVE_GMF_AUDIO_PARAM 0
#endif
#if __has_include("encoder/impl/esp_opus_enc.h")
#include "encoder/impl/esp_opus_enc.h"
#endif
#if __has_include("encoder/esp_video_enc_default.h")
#include "encoder/esp_video_enc_default.h"
#define HAVE_VIDEO_ENC_DEFAULT 1
#else
#define HAVE_VIDEO_ENC_DEFAULT 0
#endif
#else
#define HAVE_AUDIO_ENC_DEFAULT 0
#define HAVE_VIDEO_ENC_DEFAULT 0
#endif
#include "driver/i2c.h"
#include "esp_timer.h"
#ifdef __has_include
#if __has_include("driver/i2c_master.h")
#include "driver/i2c_master.h"
#define HAVE_I2C_MASTER 1
#else
#define HAVE_I2C_MASTER 0
#endif
#else
#define HAVE_I2C_MASTER 0
#endif
#ifdef __has_include
#if __has_include("esp_gmf_app_setup_peripheral.h")
#include "esp_gmf_app_setup_peripheral.h"
#define HAVE_GMF_APP_SETUP 1
#else
#define HAVE_GMF_APP_SETUP 0
#endif
#if __has_include("codec_board.h")
#include "codec_board.h"
#define HAVE_CODEC_BOARD 1
#else
#define HAVE_CODEC_BOARD 0
#endif
#if __has_include("codec_init.h")
#include "codec_init.h"
#define HAVE_CODEC_INIT 1
#else
#define HAVE_CODEC_INIT 0
#endif
#else
#define HAVE_GMF_APP_SETUP 0
#define HAVE_CODEC_BOARD 0
#define HAVE_CODEC_INIT 0
#endif

static const char *TAG = "stream_video_capture";


static esp_capture_handle_t s_capture = NULL;
static esp_capture_sink_handle_t s_sink = NULL;
static struct {
    bool     enable;
    uint32_t sample_rate;
    uint32_t bitrate;
    uint8_t  channels;
    uint8_t  bits;
} s_audio_enc_cfg = {0};

#ifdef CONFIG_STREAM_AUDIO_DEBUG_MONITOR
static void audio_monitor_task(void *arg)
{
    esp_capture_sink_handle_t sink = (esp_capture_sink_handle_t)arg;
    const TickType_t start = xTaskGetTickCount();
    TickType_t last_log = start;
    uint32_t frames = 0;
    uint32_t bytes = 0;

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(3000)) {
        esp_capture_stream_frame_t frame = {
            .stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO,
        };
        esp_capture_err_t err = esp_capture_sink_acquire_frame(sink, &frame, true);
        if (err == ESP_CAPTURE_ERR_OK) {
            frames++;
            bytes += frame.size;
            esp_capture_sink_release_frame(sink, &frame);
        }
        if ((xTaskGetTickCount() - last_log) >= pdMS_TO_TICKS(1000)) {
            ESP_LOGI(TAG, "Audio monitor: frames=%" PRIu32 " bytes=%" PRIu32, frames, bytes);
            last_log = xTaskGetTickCount();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    ESP_LOGI(TAG, "Audio monitor done: frames=%" PRIu32 " bytes=%" PRIu32, frames, bytes);
    vTaskDelete(NULL);
}
#endif

static void capture_thread_scheduler(const char *name, esp_capture_thread_schedule_cfg_t *cfg)
{
    if (!name || !cfg) {
        return;
    }
    if (strcmp(name, "venc_0") == 0) {
        cfg->stack_size = CONFIG_STREAM_VIDEO_VENC_TASK_STACK;
        cfg->stack_in_ext = true;
        cfg->priority = 2;
        cfg->core_id = 0;
        ESP_LOGI(TAG, "venc_0 stack=%" PRIu32 " (ext=%u)", cfg->stack_size, cfg->stack_in_ext);
    } else if (strcmp(name, "venc_1") == 0) {
        cfg->stack_size = CONFIG_STREAM_VIDEO_VENC_TASK_STACK;
        cfg->stack_in_ext = true;
        cfg->priority = 2;
        cfg->core_id = 1;
        ESP_LOGI(TAG, "venc_1 stack=%" PRIu32 " (ext=%u)", cfg->stack_size, cfg->stack_in_ext);
    } else if (strcmp(name, "aenc_0") == 0) {
        cfg->stack_size = 32 * 1024;
        cfg->priority = 3;
        cfg->core_id = 1;
    } else if (strcmp(name, "AUD_SRC") == 0) {
        // Keep audio source responsive; low priority can starve frames.
        cfg->priority = 15;
    }
}

static esp_capture_err_t capture_event_cb(esp_capture_event_t event, void *ctx)
{
    (void)ctx;
    if (event != ESP_CAPTURE_EVENT_AUDIO_PIPELINE_BUILT) {
        return ESP_CAPTURE_ERR_OK;
    }
    if (!s_audio_enc_cfg.enable || s_sink == NULL) {
        return ESP_CAPTURE_ERR_OK;
    }
#if defined(CONFIG_STREAM_AUDIO_AGC_ENABLE) && HAVE_GMF_AUDIO_PARAM
    esp_gmf_element_handle_t alc_el = NULL;
    if (esp_capture_sink_get_element_by_tag(s_sink, ESP_CAPTURE_STREAM_TYPE_AUDIO, "aud_alc", &alc_el) == ESP_CAPTURE_ERR_OK && alc_el != NULL) {
        esp_gmf_err_t agc_ret = esp_gmf_audio_param_set_alc_gain(alc_el, (float)CONFIG_STREAM_AUDIO_AGC_GAIN_DB);
        if (agc_ret == ESP_GMF_ERR_OK) {
            ESP_LOGI(TAG, "AGC enabled: base gain=%d dB", CONFIG_STREAM_AUDIO_AGC_GAIN_DB);
        } else {
            ESP_LOGW(TAG, "Failed to set AGC gain (ret=%d)", (int)agc_ret);
        }
    } else {
        ESP_LOGW(TAG, "AGC enabled but ALC element not found");
    }
#endif

#if defined(ESP_OPUS_ENC_CONFIG_DEFAULT) && defined(ESP_AUDIO_TYPE_OPUS)
    esp_gmf_element_handle_t aenc_el = NULL;
    if (esp_capture_sink_get_element_by_tag(s_sink, ESP_CAPTURE_STREAM_TYPE_AUDIO, "aud_enc", &aenc_el) != ESP_CAPTURE_ERR_OK || aenc_el == NULL) {
        ESP_LOGW(TAG, "Audio encoder element not found; cannot set Opus config");
        return ESP_CAPTURE_ERR_OK;
    }
    esp_opus_enc_config_t opus_cfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
    opus_cfg.sample_rate = (int)s_audio_enc_cfg.sample_rate;
    opus_cfg.channel = (int)s_audio_enc_cfg.channels;
    opus_cfg.bits_per_sample = (int)s_audio_enc_cfg.bits;
    if (s_audio_enc_cfg.bitrate > 0) {
        opus_cfg.bitrate = (int)s_audio_enc_cfg.bitrate;
    }
    opus_cfg.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_10_MS;
    opus_cfg.application_mode = ESP_OPUS_ENC_APPLICATION_VOIP;
    opus_cfg.complexity = 7; // improve quality vs default 0
    opus_cfg.enable_dtx = false;
    opus_cfg.enable_vbr = false; // force CBR to avoid sparse frames

    esp_audio_enc_config_t enc_cfg = {
        .type = ESP_AUDIO_TYPE_OPUS,
        .cfg = &opus_cfg,
        .cfg_sz = sizeof(opus_cfg),
    };
    esp_gmf_err_t ret = esp_gmf_audio_enc_reconfig(aenc_el, &enc_cfg);
    if (ret == ESP_GMF_ERR_OK) {
        ESP_LOGI(TAG, "Opus config forced: 10ms frames, dtx=0, vbr=0");
    } else {
        ESP_LOGW(TAG, "Failed to force Opus config (ret=%d)", (int)ret);
    }
#else
    ESP_LOGW(TAG, "Opus config API not available; skip 10ms/DTX settings");
#endif
    return ESP_CAPTURE_ERR_OK;
}
#ifndef STREAM_CODEC_BOARD_TYPE
#ifdef CONFIG_STREAM_CODEC_BOARD_TYPE
#define STREAM_CODEC_BOARD_TYPE CONFIG_STREAM_CODEC_BOARD_TYPE
#else
#define STREAM_CODEC_BOARD_TYPE "ESP32_S3_N16R8"
#endif
#endif

#if defined(CONFIG_STREAM_VIDEO_BOARD_XIAO_ESP32S3_SENSE)
// XIAO ESP32-S3 Sense camera pin map (Seeed Studio wiki).
#define STREAM_CAM_PIN_PWDN -1
#define STREAM_CAM_PIN_RESET -1
#define STREAM_CAM_PIN_XCLK 10
#define STREAM_CAM_PIN_D0 15  // Y2
#define STREAM_CAM_PIN_D1 17  // Y3
#define STREAM_CAM_PIN_D2 18  // Y4
#define STREAM_CAM_PIN_D3 16  // Y5
#define STREAM_CAM_PIN_D4 14  // Y6
#define STREAM_CAM_PIN_D5 12  // Y7
#define STREAM_CAM_PIN_D6 11  // Y8
#define STREAM_CAM_PIN_D7 48  // Y9
#define STREAM_CAM_PIN_VSYNC 38
#define STREAM_CAM_PIN_HREF 47
#define STREAM_CAM_PIN_PCLK 13
#define STREAM_CAM_I2C_PORT I2C_NUM_0
#define STREAM_CAM_I2C_SDA 40
#define STREAM_CAM_I2C_SCL 39
#endif

#ifndef STREAM_CAM_PIN_PWDN
// Default pin map matches ESP32-S3 WROOM camera wiring.
#define STREAM_CAM_PIN_PWDN 38
#endif
#ifndef STREAM_CAM_PIN_RESET
#define STREAM_CAM_PIN_RESET -1
#endif
#ifndef STREAM_CAM_PIN_XCLK
#define STREAM_CAM_PIN_XCLK 15
#endif
#ifndef STREAM_CAM_PIN_D0
#define STREAM_CAM_PIN_D0 11
#endif
#ifndef STREAM_CAM_PIN_D1
#define STREAM_CAM_PIN_D1 9
#endif
#ifndef STREAM_CAM_PIN_D2
#define STREAM_CAM_PIN_D2 8
#endif
#ifndef STREAM_CAM_PIN_D3
#define STREAM_CAM_PIN_D3 10
#endif
#ifndef STREAM_CAM_PIN_D4
#define STREAM_CAM_PIN_D4 12
#endif
#ifndef STREAM_CAM_PIN_D5
#define STREAM_CAM_PIN_D5 18
#endif
#ifndef STREAM_CAM_PIN_D6
#define STREAM_CAM_PIN_D6 17
#endif
#ifndef STREAM_CAM_PIN_D7
#define STREAM_CAM_PIN_D7 16
#endif
#ifndef STREAM_CAM_PIN_VSYNC
#define STREAM_CAM_PIN_VSYNC 6
#endif
#ifndef STREAM_CAM_PIN_HREF
#define STREAM_CAM_PIN_HREF 7
#endif
#ifndef STREAM_CAM_PIN_PCLK
#define STREAM_CAM_PIN_PCLK 13
#endif
#ifndef STREAM_CAM_I2C_PORT
#define STREAM_CAM_I2C_PORT I2C_NUM_0
#endif
#ifndef STREAM_CAM_I2C_SDA
#define STREAM_CAM_I2C_SDA 4
#endif
#ifndef STREAM_CAM_I2C_SCL
#define STREAM_CAM_I2C_SCL 5
#endif
#ifndef STREAM_VIDEO_WIDTH
#ifdef CONFIG_STREAM_VIDEO_WIDTH
#define STREAM_VIDEO_WIDTH CONFIG_STREAM_VIDEO_WIDTH
#else
#define STREAM_VIDEO_WIDTH 160
#endif
#endif
#ifndef STREAM_VIDEO_HEIGHT
#ifdef CONFIG_STREAM_VIDEO_HEIGHT
#define STREAM_VIDEO_HEIGHT CONFIG_STREAM_VIDEO_HEIGHT
#else
#define STREAM_VIDEO_HEIGHT 120
#endif
#endif
#ifndef STREAM_VIDEO_FPS
#ifdef CONFIG_STREAM_VIDEO_FPS
#define STREAM_VIDEO_FPS CONFIG_STREAM_VIDEO_FPS
#else
#define STREAM_VIDEO_FPS 10
#endif
#endif

#ifndef STREAM_AUDIO_ENABLE
#ifdef CONFIG_STREAM_AUDIO_ENABLE
#define STREAM_AUDIO_ENABLE CONFIG_STREAM_AUDIO_ENABLE
#else
#define STREAM_AUDIO_ENABLE 0
#endif
#endif
#ifndef STREAM_AUDIO_SAMPLE_RATE
#ifdef CONFIG_STREAM_AUDIO_SAMPLE_RATE
#define STREAM_AUDIO_SAMPLE_RATE CONFIG_STREAM_AUDIO_SAMPLE_RATE
#else
#define STREAM_AUDIO_SAMPLE_RATE 16000
#endif
#endif
#ifndef STREAM_AUDIO_CHANNELS
#ifdef CONFIG_STREAM_AUDIO_CHANNELS
#define STREAM_AUDIO_CHANNELS CONFIG_STREAM_AUDIO_CHANNELS
#else
#define STREAM_AUDIO_CHANNELS 2
#endif
#endif
#ifndef STREAM_AUDIO_BITS_PER_SAMPLE
#ifdef CONFIG_STREAM_AUDIO_BITS_PER_SAMPLE
#define STREAM_AUDIO_BITS_PER_SAMPLE CONFIG_STREAM_AUDIO_BITS_PER_SAMPLE
#else
#define STREAM_AUDIO_BITS_PER_SAMPLE 16
#endif
#endif
#ifndef STREAM_AUDIO_BITRATE
#ifdef CONFIG_STREAM_AUDIO_BITRATE
#define STREAM_AUDIO_BITRATE CONFIG_STREAM_AUDIO_BITRATE
#else
#define STREAM_AUDIO_BITRATE 32000
#endif
#endif
#ifndef STREAM_AUDIO_INPUT_GAIN
#ifdef CONFIG_STREAM_AUDIO_INPUT_GAIN
#define STREAM_AUDIO_INPUT_GAIN CONFIG_STREAM_AUDIO_INPUT_GAIN
#else
#define STREAM_AUDIO_INPUT_GAIN 30
#endif
#endif
#ifndef STREAM_AUDIO_LEVEL_PROBE_MS
#ifdef CONFIG_STREAM_AUDIO_LEVEL_PROBE_MS
#define STREAM_AUDIO_LEVEL_PROBE_MS CONFIG_STREAM_AUDIO_LEVEL_PROBE_MS
#else
#define STREAM_AUDIO_LEVEL_PROBE_MS 500
#endif
#endif
#ifndef STREAM_AUDIO_I2S_MODE
#if defined(CONFIG_STREAM_AUDIO_I2S_MODE_TDM)
#define STREAM_AUDIO_I2S_MODE ESP_GMF_APP_I2S_MODE_TDM
#else
#define STREAM_AUDIO_I2S_MODE ESP_GMF_APP_I2S_MODE_STD
#endif
#endif

#if defined(CONFIG_STREAM_VIDEO_BOARD_GENERIC_ESP32S3) || defined(CONFIG_STREAM_VIDEO_BOARD_XIAO_ESP32S3_SENSE)
#define STREAM_USE_LOCAL_CAM_PINS 1
#else
#define STREAM_USE_LOCAL_CAM_PINS 0
#endif

#if STREAM_USE_LOCAL_CAM_PINS
static esp_err_t init_camera_i2c(void)
{
#if HAVE_I2C_MASTER
    static bool s_i2c_inited = false;
    if (s_i2c_inited) {
        return ESP_OK;
    }
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = STREAM_CAM_I2C_PORT,
        .sda_io_num = STREAM_CAM_I2C_SDA,
        .scl_io_num = STREAM_CAM_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle = NULL;
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus_handle);
    if (err == ESP_ERR_INVALID_STATE) {
        s_i2c_inited = true;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }
    s_i2c_inited = true;
    return ESP_OK;
#else
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = STREAM_CAM_I2C_SDA,
        .scl_io_num = STREAM_CAM_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    esp_err_t err = i2c_param_config(STREAM_CAM_I2C_PORT, &conf);
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_driver_install(STREAM_CAM_I2C_PORT, conf.mode, 0, 0, 0);
    if (err == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return err;
#endif
}
#endif

static void init_codec_board_if_available(void)
{
#if HAVE_CODEC_BOARD
    ESP_LOGI(TAG, "Audio codec board type: %s", STREAM_CODEC_BOARD_TYPE);
    set_codec_board_type(STREAM_CODEC_BOARD_TYPE);
#if HAVE_CODEC_INIT
    codec_init_cfg_t cfg = {
        .in_mode = CODEC_I2S_MODE_STD,
        .out_mode = CODEC_I2S_MODE_STD,
        .in_use_tdm = false,
        .reuse_dev = false,
    };
#if defined(CONFIG_STREAM_AUDIO_I2S_MODE_TDM)
    cfg.in_mode = CODEC_I2S_MODE_TDM;
    cfg.in_use_tdm = true;
#endif
    if (strcmp(STREAM_CODEC_BOARD_TYPE, "XIAO_ESP32S3_Sense") == 0) {
        cfg.in_mode = CODEC_I2S_MODE_PDM;
        cfg.out_mode = CODEC_I2S_MODE_NONE;
        cfg.in_use_tdm = false;
        ESP_LOGI(TAG, "Using PDM input for XIAO ESP32S3 Sense mic");
    }
    init_codec(&cfg);
#endif
#endif
}

static esp_capture_video_src_if_t *create_camera_source(void)
{
#if STREAM_USE_LOCAL_CAM_PINS
    if (init_camera_i2c() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init camera I2C");
        return NULL;
    }
    ESP_LOGI(TAG, "Camera I2C: port=%d sda=%d scl=%d",
             (int)STREAM_CAM_I2C_PORT, STREAM_CAM_I2C_SDA, STREAM_CAM_I2C_SCL);
    ESP_LOGI(TAG,
             "Camera pins: pwdn=%d reset=%d xclk=%d pclk=%d vsync=%d href=%d "
             "d0=%d d1=%d d2=%d d3=%d d4=%d d5=%d d6=%d d7=%d",
             STREAM_CAM_PIN_PWDN, STREAM_CAM_PIN_RESET, STREAM_CAM_PIN_XCLK,
             STREAM_CAM_PIN_PCLK, STREAM_CAM_PIN_VSYNC, STREAM_CAM_PIN_HREF,
             STREAM_CAM_PIN_D0, STREAM_CAM_PIN_D1, STREAM_CAM_PIN_D2,
             STREAM_CAM_PIN_D3, STREAM_CAM_PIN_D4, STREAM_CAM_PIN_D5,
             STREAM_CAM_PIN_D6, STREAM_CAM_PIN_D7);
    esp_capture_video_dvp_src_cfg_t dvp_config = { 0 };
    dvp_config.buf_count = 2;
    dvp_config.reset_pin = STREAM_CAM_PIN_RESET;
    dvp_config.pwr_pin = STREAM_CAM_PIN_PWDN;
    dvp_config.data[0] = STREAM_CAM_PIN_D0;
    dvp_config.data[1] = STREAM_CAM_PIN_D1;
    dvp_config.data[2] = STREAM_CAM_PIN_D2;
    dvp_config.data[3] = STREAM_CAM_PIN_D3;
    dvp_config.data[4] = STREAM_CAM_PIN_D4;
    dvp_config.data[5] = STREAM_CAM_PIN_D5;
    dvp_config.data[6] = STREAM_CAM_PIN_D6;
    dvp_config.data[7] = STREAM_CAM_PIN_D7;
    dvp_config.vsync_pin = STREAM_CAM_PIN_VSYNC;
    dvp_config.href_pin = STREAM_CAM_PIN_HREF;
    dvp_config.pclk_pin = STREAM_CAM_PIN_PCLK;
    dvp_config.xclk_pin = STREAM_CAM_PIN_XCLK;
    dvp_config.xclk_freq = 20000000;
    dvp_config.i2c_port = STREAM_CAM_I2C_PORT;
    return esp_capture_new_video_dvp_src(&dvp_config);
#else
    camera_cfg_t cam_pin_cfg = {};
    if (get_camera_cfg(&cam_pin_cfg) != 0) {
        ESP_LOGE(TAG, "Failed to get camera config");
        return NULL;
    }

    if (cam_pin_cfg.type != CAMERA_TYPE_DVP) {
        ESP_LOGE(TAG, "Only DVP camera supported in this example");
        return NULL;
    }

    esp_capture_video_dvp_src_cfg_t dvp_config = { 0 };
    dvp_config.buf_count = 2;
    dvp_config.reset_pin = cam_pin_cfg.reset;
    dvp_config.pwr_pin = cam_pin_cfg.pwr;
    dvp_config.data[0] = cam_pin_cfg.data[0];
    dvp_config.data[1] = cam_pin_cfg.data[1];
    dvp_config.data[2] = cam_pin_cfg.data[2];
    dvp_config.data[3] = cam_pin_cfg.data[3];
    dvp_config.data[4] = cam_pin_cfg.data[4];
    dvp_config.data[5] = cam_pin_cfg.data[5];
    dvp_config.data[6] = cam_pin_cfg.data[6];
    dvp_config.data[7] = cam_pin_cfg.data[7];
    dvp_config.vsync_pin = cam_pin_cfg.vsync;
    dvp_config.href_pin = cam_pin_cfg.href;
    dvp_config.pclk_pin = cam_pin_cfg.pclk;
    dvp_config.xclk_pin = cam_pin_cfg.xclk;
    dvp_config.xclk_freq = 20000000;
    dvp_config.i2c_port = 0;

    return esp_capture_new_video_dvp_src(&dvp_config);
#endif
}

esp_err_t stream_video_default_capture_run_resolution_test(void)
{
#if !HAVE_VIDEO_ENC_DEFAULT
    ESP_LOGE(TAG, "Resolution test requires esp_video_enc_default");
    return ESP_ERR_NOT_SUPPORTED;
#else
    esp_video_enc_register_default();
#endif
    ESP_LOGI(TAG, "Resolution test: installing capture thread scheduler");
    esp_capture_set_thread_scheduler(capture_thread_scheduler);

    static const struct {
        uint16_t width;
        uint16_t height;
    } test_sizes[] = {
        {320, 240},
        {400, 296},
        {426, 240},
        {480, 270},
        {480, 320},
        {640, 360},
        {640, 480},
        {800, 600},
        {1024, 768},
    };

    ESP_LOGI(TAG, "Resolution test: fps=%u bitrate=%u",
             (unsigned)STREAM_VIDEO_FPS, (unsigned)CONFIG_STREAM_VIDEO_BITRATE);

    for (size_t i = 0; i < sizeof(test_sizes) / sizeof(test_sizes[0]); ++i) {
        const uint16_t width = test_sizes[i].width;
        const uint16_t height = test_sizes[i].height;
        ESP_LOGI(TAG, "Resolution test: %ux%u@%u", (unsigned)width, (unsigned)height,
                 (unsigned)STREAM_VIDEO_FPS);

        esp_capture_video_src_if_t *video_src = create_camera_source();
        if (!video_src) {
            ESP_LOGE(TAG, "Resolution test: failed to create camera source");
            continue;
        }

        if (!video_src->set_fixed_caps) {
            ESP_LOGE(TAG, "Resolution test: set_fixed_caps not supported");
            continue;
        }

        esp_capture_video_info_t fixed_caps = {
#if defined(CONFIG_STREAM_VIDEO_CAMERA_FMT_YUV420)
            .format_id = ESP_CAPTURE_FMT_ID_YUV420,
#else
            .format_id = ESP_CAPTURE_FMT_ID_YUV422,
#endif
            .width = width,
            .height = height,
            .fps = STREAM_VIDEO_FPS,
        };

        esp_capture_err_t cap_err = video_src->set_fixed_caps(video_src, &fixed_caps);
        if (cap_err != ESP_CAPTURE_ERR_OK) {
            ESP_LOGW(TAG, "Resolution test: set_fixed_caps failed err=%d", (int)cap_err);
            continue;
        }

        esp_capture_handle_t capture = NULL;
        esp_capture_cfg_t cfg = {
            .sync_mode = ESP_CAPTURE_SYNC_MODE_NONE,
            .audio_src = NULL,
            .video_src = video_src,
        };
        if (esp_capture_open(&cfg, &capture) != ESP_CAPTURE_ERR_OK || !capture) {
            ESP_LOGW(TAG, "Resolution test: capture open failed");
            continue;
        }

        esp_capture_sink_handle_t sink = NULL;
        esp_capture_sink_cfg_t sink_cfg = {
            .audio_info = {
                .format_id = ESP_CAPTURE_FMT_ID_NONE,
                .sample_rate = 0,
                .channel = 0,
                .bits_per_sample = 0,
            },
            .video_info = {
                .format_id = ESP_CAPTURE_FMT_ID_H264,
                .width = width,
                .height = height,
                .fps = STREAM_VIDEO_FPS,
            },
        };
        if (esp_capture_sink_setup(capture, 0, &sink_cfg, &sink) != ESP_CAPTURE_ERR_OK || !sink) {
            ESP_LOGW(TAG, "Resolution test: sink setup failed");
            esp_capture_close(capture);
            continue;
        }

        esp_capture_sink_set_bitrate(sink, ESP_CAPTURE_STREAM_TYPE_VIDEO, CONFIG_STREAM_VIDEO_BITRATE);
        esp_capture_sink_enable(sink, ESP_CAPTURE_RUN_MODE_ALWAYS);
        if (esp_capture_start(capture) != ESP_CAPTURE_ERR_OK) {
            ESP_LOGW(TAG, "Resolution test: capture start failed");
            esp_capture_close(capture);
            continue;
        }

        int ok_frames = 0;
        esp_capture_stream_frame_t frame = {
            .stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO,
        };
        for (int attempt = 0; attempt < 6; ++attempt) {
            esp_capture_err_t err = esp_capture_sink_acquire_frame(sink, &frame, false);
            if (err == ESP_CAPTURE_ERR_OK) {
                ok_frames++;
                esp_capture_sink_release_frame(sink, &frame);
                if (ok_frames >= 3) {
                    break;
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(60));
            }
        }

        if (ok_frames >= 3) {
            ESP_LOGI(TAG, "Resolution test: PASS (%ux%u)", (unsigned)width, (unsigned)height);
        } else {
            ESP_LOGW(TAG, "Resolution test: FAIL (%ux%u)", (unsigned)width, (unsigned)height);
        }

        esp_capture_stop(capture);
        esp_capture_close(capture);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    ESP_LOGI(TAG, "Resolution test complete");
    return ESP_OK;
}

stream_video_error_t stream_video_default_capture_prepare(void *user_data,
                                                          esp_capture_sink_handle_t *sink_out)
{
    (void)user_data;
    if (!sink_out) {
        return STREAM_VIDEO_ERR_INVALID_ARG;
    }
    if (s_capture) {
        *sink_out = s_sink;
        return STREAM_VIDEO_ERR_OK;
    }

    init_codec_board_if_available();

    ESP_LOGI(TAG, "Installing capture thread scheduler");
    esp_capture_set_thread_scheduler(capture_thread_scheduler);
    ESP_LOGI(TAG, "Config: video=%ux%u@%u fmt=%s",
             (unsigned)STREAM_VIDEO_WIDTH,
             (unsigned)STREAM_VIDEO_HEIGHT,
             (unsigned)STREAM_VIDEO_FPS,
#if defined(CONFIG_STREAM_VIDEO_CAMERA_FMT_YUV420)
             "YUV420"
#else
             "YUV422"
#endif
    );

    bool enable_audio = false;
    bool want_audio = STREAM_AUDIO_ENABLE ? true : false;
    esp_capture_audio_src_if_t *audio_src = NULL;
    uint32_t audio_sample_rate = STREAM_AUDIO_SAMPLE_RATE;
    uint8_t audio_channels = STREAM_AUDIO_CHANNELS;
    uint8_t audio_bits = STREAM_AUDIO_BITS_PER_SAMPLE;

#if HAVE_AUDIO_ENC_DEFAULT
    esp_audio_enc_register_default();
#else
    ESP_LOGW(TAG, "esp_audio_enc_default not available; audio encode may fail");
#endif

#if HAVE_VIDEO_ENC_DEFAULT
    esp_video_enc_register_default();
#else
    ESP_LOGW(TAG, "esp_video_enc_default not available; video encode may fail");
#endif

    esp_capture_video_src_if_t *video_src = create_camera_source();
    if (!video_src) {
        ESP_LOGE(TAG, "Failed to create camera source");
        return ESP_FAIL;
    }
    esp_capture_video_info_t fixed_caps = {
#if defined(CONFIG_STREAM_VIDEO_CAMERA_FMT_YUV420)
        .format_id = ESP_CAPTURE_FMT_ID_YUV420,
#else
        .format_id = ESP_CAPTURE_FMT_ID_YUV422,
#endif
        .width = STREAM_VIDEO_WIDTH,
        .height = STREAM_VIDEO_HEIGHT,
        .fps = STREAM_VIDEO_FPS,
    };
    bool use_fixed_caps = false;
#if defined(CONFIG_STREAM_VIDEO_USE_FIXED_CAPS)
    use_fixed_caps = true;
#endif
    if (use_fixed_caps && video_src->set_fixed_caps) {
        esp_capture_err_t cap_err = video_src->set_fixed_caps(video_src, &fixed_caps);
        if (cap_err != ESP_CAPTURE_ERR_OK) {
            ESP_LOGW(TAG, "Failed to set camera fixed caps: err=%d", (int)cap_err);
        } else {
            const char *fmt_name =
                (fixed_caps.format_id == ESP_CAPTURE_FMT_ID_YUV420) ? "YUV420" :
                (fixed_caps.format_id == ESP_CAPTURE_FMT_ID_YUV422) ? "YUV422" :
                "UNKNOWN";
            ESP_LOGI(TAG, "Camera fixed caps set: %s %ux%u@%u",
                     fmt_name,
                     (unsigned)fixed_caps.width,
                     (unsigned)fixed_caps.height,
                     (unsigned)fixed_caps.fps);
        }
    } else if (!use_fixed_caps) {
        ESP_LOGI(TAG, "Camera fixed caps disabled; using driver defaults");
    }

#if HAVE_CODEC_BOARD && HAVE_CAPTURE_AUDIO_DEV
    if (want_audio && strcmp(STREAM_CODEC_BOARD_TYPE, "XIAO_ESP32S3_Sense") == 0) {
        if (audio_channels != 1) {
            ESP_LOGW(TAG, "PDM mic is mono; forcing audio channels to 1");
            audio_channels = 1;
        }
    }
    if (want_audio) {
        esp_codec_dev_handle_t record_handle = get_record_handle();
        if (!record_handle) {
            ESP_LOGW(TAG, "Failed to get record handle; continuing with video only");
        } else {
            if (esp_codec_dev_set_in_gain(record_handle, (float)STREAM_AUDIO_INPUT_GAIN) == ESP_CODEC_DEV_OK) {
                ESP_LOGI(TAG, "Audio input gain set to %d dB", STREAM_AUDIO_INPUT_GAIN);
            } else {
                ESP_LOGW(TAG, "Failed to set audio input gain to %d dB", STREAM_AUDIO_INPUT_GAIN);
            }
#ifdef CONFIG_STREAM_AUDIO_LEVEL_PROBE
            if (STREAM_AUDIO_BITS_PER_SAMPLE == 16) {
                int16_t probe_buf[256];
                size_t probe_samples = 0;
                double sum_sq = 0.0;
                int16_t peak = 0;
                int64_t start_us = esp_timer_get_time();
                int64_t end_us = start_us + ((int64_t)STREAM_AUDIO_LEVEL_PROBE_MS * 1000LL);
                while (esp_timer_get_time() < end_us) {
                    int got = esp_codec_dev_read(record_handle, probe_buf, sizeof(probe_buf));
                    if (got <= 0) {
                        vTaskDelay(pdMS_TO_TICKS(10));
                        continue;
                    }
                    int samples = got / (int)sizeof(int16_t);
                    for (int i = 0; i < samples; ++i) {
                        int16_t s = probe_buf[i];
                        int16_t abs_s = (s >= 0) ? s : (int16_t)(-s);
                        if (abs_s > peak) {
                            peak = abs_s;
                        }
                        sum_sq += (double)s * (double)s;
                    }
                    probe_samples += (size_t)samples;
                }
                if (probe_samples > 0) {
                    double rms = sqrt(sum_sq / (double)probe_samples);
                    ESP_LOGI(TAG, "Mic probe: samples=%u rms=%.1f peak=%d",
                             (unsigned)probe_samples, rms, (int)peak);
                } else {
                    ESP_LOGW(TAG, "Mic probe: no samples captured");
                }
            } else {
                ESP_LOGW(TAG, "Mic probe: unsupported bits_per_sample=%u",
                         (unsigned)STREAM_AUDIO_BITS_PER_SAMPLE);
            }
#endif
            esp_capture_audio_dev_src_cfg_t audio_cfg = {
                .record_handle = record_handle,
            };
            audio_src = esp_capture_new_audio_dev_src(&audio_cfg);
            if (!audio_src) {
                ESP_LOGW(TAG, "Failed to create audio source; continuing with video only");
            } else {
                enable_audio = true;
            }
        }
    } else {
        ESP_LOGI(TAG, "Audio capture disabled by config");
    }
#else
    if (want_audio) {
        ESP_LOGW(TAG, "Audio capture unavailable (missing codec_board or audio dev source)");
    } else {
        ESP_LOGI(TAG, "Audio capture disabled by config");
    }
#endif

    esp_capture_cfg_t cfg = {
        .sync_mode = enable_audio ? ESP_CAPTURE_SYNC_MODE_AUDIO : ESP_CAPTURE_SYNC_MODE_NONE,
        .audio_src = enable_audio ? audio_src : NULL,
        .video_src = video_src,
    };
    if (esp_capture_open(&cfg, &s_capture) != ESP_CAPTURE_ERR_OK || !s_capture) {
        ESP_LOGE(TAG, "Failed to open capture");
        return STREAM_VIDEO_ERR_FAIL;
    }

    ESP_LOGI(TAG, "Audio config: rate=%u channels=%u bits=%u",
             (unsigned)audio_sample_rate,
             (unsigned)audio_channels,
             (unsigned)audio_bits);
    esp_capture_sink_cfg_t sink_cfg = {
        .audio_info = {
            .format_id = enable_audio ? ESP_CAPTURE_FMT_ID_OPUS : ESP_CAPTURE_FMT_ID_NONE,
            .sample_rate = enable_audio ? audio_sample_rate : 0,
            .channel = enable_audio ? audio_channels : 0,
            .bits_per_sample = enable_audio ? audio_bits : 0,
        },
        .video_info = {
            .format_id = ESP_CAPTURE_FMT_ID_H264,
            .width = STREAM_VIDEO_WIDTH,
            .height = STREAM_VIDEO_HEIGHT,
            .fps = STREAM_VIDEO_FPS,
        },
    };
    if (esp_capture_sink_setup(s_capture, 0, &sink_cfg, &s_sink) != ESP_CAPTURE_ERR_OK || !s_sink) {
        ESP_LOGE(TAG, "Failed to setup capture sink");
        return STREAM_VIDEO_ERR_FAIL;
    }
#if defined(CONFIG_STREAM_AUDIO_AGC_ENABLE) && HAVE_GMF_ALC
    if (enable_audio) {
        esp_gmf_element_handle_t alc_el = NULL;
        esp_ae_alc_cfg_t alc_cfg = DEFAULT_ESP_GMF_ALC_CONFIG();
        alc_cfg.sample_rate = audio_sample_rate;
        alc_cfg.channel = audio_channels;
        alc_cfg.bits_per_sample = audio_bits;
        if (esp_gmf_alc_init(&alc_cfg, &alc_el) == ESP_GMF_ERR_OK && alc_el) {
            if (esp_capture_register_element(s_capture, ESP_CAPTURE_STREAM_TYPE_AUDIO, alc_el) == ESP_CAPTURE_ERR_OK) {
                const char *audio_pipe[] = {"aud_ch_cvt", "aud_rate_cvt", "aud_bit_cvt", "aud_alc", "aud_enc"};
                esp_capture_err_t pipe_ret = esp_capture_sink_build_pipeline(s_sink, ESP_CAPTURE_STREAM_TYPE_AUDIO,
                                                                             audio_pipe, (uint8_t)(sizeof(audio_pipe) / sizeof(audio_pipe[0])));
                if (pipe_ret == ESP_CAPTURE_ERR_OK) {
                    ESP_LOGI(TAG, "AGC pipeline enabled");
                } else {
                    ESP_LOGW(TAG, "Failed to enable AGC pipeline (err=%d)", (int)pipe_ret);
                }
            } else {
                ESP_LOGW(TAG, "Failed to register ALC element");
            }
        } else {
            ESP_LOGW(TAG, "Failed to init ALC element");
        }
    }
#endif
    s_audio_enc_cfg.enable = enable_audio;
    s_audio_enc_cfg.sample_rate = audio_sample_rate;
    s_audio_enc_cfg.channels = (uint8_t)audio_channels;
    s_audio_enc_cfg.bits = (uint8_t)audio_bits;
    s_audio_enc_cfg.bitrate = 0;
#ifdef CONFIG_STREAM_AUDIO_BITRATE
    s_audio_enc_cfg.bitrate = CONFIG_STREAM_AUDIO_BITRATE;
#endif
    esp_capture_set_event_cb(s_capture, capture_event_cb, NULL);
#ifdef CONFIG_STREAM_VIDEO_BITRATE
    if (esp_capture_sink_set_bitrate(s_sink, ESP_CAPTURE_STREAM_TYPE_VIDEO, CONFIG_STREAM_VIDEO_BITRATE) == ESP_CAPTURE_ERR_OK) {
        ESP_LOGI(TAG, "Video bitrate set to %u bps", (unsigned)CONFIG_STREAM_VIDEO_BITRATE);
    } else {
        ESP_LOGW(TAG, "Failed to set video bitrate to %u bps", (unsigned)CONFIG_STREAM_VIDEO_BITRATE);
    }
#endif
#ifdef CONFIG_STREAM_AUDIO_BITRATE
    if (enable_audio) {
        if (esp_capture_sink_set_bitrate(s_sink, ESP_CAPTURE_STREAM_TYPE_AUDIO, CONFIG_STREAM_AUDIO_BITRATE) == ESP_CAPTURE_ERR_OK) {
            ESP_LOGI(TAG, "Audio bitrate set to %u bps", (unsigned)CONFIG_STREAM_AUDIO_BITRATE);
        } else {
            ESP_LOGW(TAG, "Failed to set audio bitrate to %u bps", (unsigned)CONFIG_STREAM_AUDIO_BITRATE);
        }
    }
#endif
    esp_capture_sink_enable(s_sink, ESP_CAPTURE_RUN_MODE_ALWAYS);

    if (esp_capture_start(s_capture) != ESP_CAPTURE_ERR_OK) {
        ESP_LOGE(TAG, "Failed to start capture");
        return STREAM_VIDEO_ERR_FAIL;
    }

#ifdef CONFIG_STREAM_AUDIO_DEBUG_MONITOR
    if (enable_audio) {
        xTaskCreate(audio_monitor_task, "audio_mon", 4096, s_sink, 3, NULL);
    }
#endif

    *sink_out = s_sink;
    ESP_LOGI(TAG, "Media capture prepared (sink=%p)", (void *)s_sink);
    return STREAM_VIDEO_ERR_OK;
}

void stream_video_default_capture_stop(void *user_data)
{
    (void)user_data;
    if (s_capture) {
        esp_capture_stop(s_capture);
        esp_capture_close(s_capture);
        s_capture = NULL;
        s_sink = NULL;
    }
}
