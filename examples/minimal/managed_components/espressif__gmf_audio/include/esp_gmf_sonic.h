/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"
#include "esp_ae_sonic.h"
#include "esp_gmf_element.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DEFAULT_ESP_GMF_SONIC_CONFIG() {  \
    .sample_rate     = 48000,             \
    .channel         = 2,                 \
    .bits_per_sample = 16,                \
}

/**
 * @brief  Initializes the GMF sonic with the provided configuration
 *
 * @param[in]   config  Pointer to the sonic configuration
 * @param[out]  handle  Pointer to the sonic handle to be initialized
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_ERR_INVALID_ARG      Invalid configuration provided
 *       - ESP_GMF_ERR_MEMORY_LACK  Failed to allocate memory
 */
esp_gmf_err_t esp_gmf_sonic_init(esp_ae_sonic_cfg_t *config, esp_gmf_element_handle_t *handle);

/**
 * @brief  Set the audio speed
 *
 * @param[in]  handle  The handle of the sonic
 * @param[in]  speed   The scaling factor of audio speed.
 *                     The range of speed is [0.5, 2.0]
 *
 * @return
 *       - ESP_GMF_ERR_OK           Operation succeeded
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid input parameter
 */
esp_gmf_err_t esp_gmf_sonic_set_speed(esp_gmf_element_handle_t handle, float speed);

/**
 * @brief  Get the audio speed
 *
 * @param[in]   handle  The handle of the sonic
 * @param[out]  speed   The scaling factor of audio speed
 *
 * @return
 *       - ESP_GMF_ERR_OK           Operation succeeded
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid input parameter
 */
esp_gmf_err_t esp_gmf_sonic_get_speed(esp_gmf_element_handle_t handle, float *speed);

/**
 * @brief  Set the audio pitch
 *
 * @param[in]  handle  The handle of the sonic
 * @param[in]  pitch   The scaling factor of audio pitch.
 *                     The range of pitch is [0.5, 2.0].
 *                     If the pitch value is smaller than 1.0, the sound is deep voice;
 *                     if the pitch value is equal to 1.0, the sound is no change;
 *                     if the pitch value is gather than 1.0, the sound is sharp voice;
 *
 * @return
 *       - ESP_GMF_ERR_OK           Operation succeeded
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid input parameter
 */
esp_gmf_err_t esp_gmf_sonic_set_pitch(esp_gmf_element_handle_t handle, float pitch);

/**
 * @brief  Get the audio pitch
 *
 * @param[in]   handle  The handle of the sonic
 * @param[out]  pitch   The scaling factor of audio pitch
 *
 * @return
 *       - ESP_GMF_ERR_OK           Operation succeeded
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid input parameter
 */
esp_gmf_err_t esp_gmf_sonic_get_pitch(esp_gmf_element_handle_t handle, float *pitch);

#ifdef __cplusplus
}
#endif /* __cplusplus */
