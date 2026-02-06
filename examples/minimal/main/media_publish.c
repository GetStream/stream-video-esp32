#include "media_publish.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include <inttypes.h>

#include "esp_capture.h"
#include "esp_capture_sink.h"
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

static const char *TAG = "media_publish";


static esp_capture_handle_t s_capture = NULL;
static esp_capture_sink_handle_t s_sink = NULL;

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
    }
}
#ifndef STREAM_CODEC_BOARD_TYPE
#define STREAM_CODEC_BOARD_TYPE "ESP32_S3_N16R8"
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

#if !HAVE_CODEC_BOARD
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
    set_codec_board_type(STREAM_CODEC_BOARD_TYPE);
#if HAVE_CODEC_INIT
    codec_init_cfg_t cfg = {
        .reuse_dev = false,
    };
    init_codec(&cfg);
#endif
#endif
}

static esp_capture_video_src_if_t *create_camera_source(void)
{
#if !HAVE_CODEC_BOARD
    if (init_camera_i2c() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init camera I2C");
        return NULL;
    }
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

esp_err_t media_publish_start(stream_video_client_handle_t client)
{
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_capture) {
        return ESP_OK;
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
    esp_capture_audio_src_if_t *audio_src = NULL;
#if HAVE_GMF_APP_SETUP && HAVE_CAPTURE_AUDIO_DEV
    esp_gmf_app_codec_info_t codec_info = {
        .play_info = {
            .sample_rate = 16000,
            .channel = 2,
            .bits_per_sample = 16,
            .mode = ESP_GMF_APP_I2S_MODE_STD,
        },
        .record_info = {
            .sample_rate = 16000,
            .channel = 2,
            .bits_per_sample = 16,
#if CONFIG_IDF_TARGET_ESP32S3
            .mode = ESP_GMF_APP_I2S_MODE_TDM,
#else
            .mode = ESP_GMF_APP_I2S_MODE_STD,
#endif
        },
    };
    esp_gmf_app_setup_codec_dev(&codec_info);
#endif

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
    if (video_src->set_fixed_caps) {
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
    }

#if HAVE_GMF_APP_SETUP && HAVE_CAPTURE_AUDIO_DEV
    esp_capture_audio_dev_src_cfg_t audio_cfg = {
        .record_handle = esp_gmf_app_get_record_handle(),
    };
    audio_src = esp_capture_new_audio_dev_src(&audio_cfg);
    if (!audio_src) {
        ESP_LOGW(TAG, "Failed to create audio source; continuing with video only");
    } else {
        enable_audio = true;
    }
#else
    ESP_LOGW(TAG, "Audio capture disabled (missing esp_gmf_app_setup_peripheral or audio dev source)");
#endif

    esp_capture_cfg_t cfg = {
        .sync_mode = enable_audio ? ESP_CAPTURE_SYNC_MODE_AUDIO : ESP_CAPTURE_SYNC_MODE_NONE,
        .audio_src = enable_audio ? audio_src : NULL,
        .video_src = video_src,
    };
    if (esp_capture_open(&cfg, &s_capture) != ESP_CAPTURE_ERR_OK || !s_capture) {
        ESP_LOGE(TAG, "Failed to open capture");
        return ESP_FAIL;
    }

    esp_capture_sink_cfg_t sink_cfg = {
        .audio_info = {
            .format_id = enable_audio ? ESP_CAPTURE_FMT_ID_OPUS : ESP_CAPTURE_FMT_ID_NONE,
            .sample_rate = enable_audio ? 16000 : 0,
            .channel = enable_audio ? 2 : 0,
            .bits_per_sample = enable_audio ? 16 : 0,
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
        return ESP_FAIL;
    }
#ifdef CONFIG_STREAM_VIDEO_BITRATE
    if (esp_capture_sink_set_bitrate(s_sink, ESP_CAPTURE_STREAM_TYPE_VIDEO, CONFIG_STREAM_VIDEO_BITRATE) == ESP_CAPTURE_ERR_OK) {
        ESP_LOGI(TAG, "Video bitrate set to %u bps", (unsigned)CONFIG_STREAM_VIDEO_BITRATE);
    } else {
        ESP_LOGW(TAG, "Failed to set video bitrate to %u bps", (unsigned)CONFIG_STREAM_VIDEO_BITRATE);
    }
#endif
    esp_capture_sink_enable(s_sink, ESP_CAPTURE_RUN_MODE_ALWAYS);

    if (esp_capture_start(s_capture) != ESP_CAPTURE_ERR_OK) {
        ESP_LOGE(TAG, "Failed to start capture");
        return ESP_FAIL;
    }

    stream_video_publish_params_t params = {
        .sink = s_sink,
        .publish_audio = enable_audio,
        .publish_video = true,
    };
    ESP_LOGI(TAG, "media_publish_start: calling stream_video_start_publishing (sink=%p audio=%d video=1)",
             (void *)s_sink,
             enable_audio ? 1 : 0);
    stream_video_error_t err = stream_video_start_publishing(client, &params);
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to start publishing: %s (%d)",
                 stream_video_error_to_string(err), err);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Media capture and publishing started");
    return ESP_OK;
}

esp_err_t media_publish_stop(stream_video_client_handle_t client)
{
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    stream_video_stop_publishing(client);
    if (s_capture) {
        esp_capture_stop(s_capture);
        esp_capture_close(s_capture);
        s_capture = NULL;
        s_sink = NULL;
    }
    return ESP_OK;
}
