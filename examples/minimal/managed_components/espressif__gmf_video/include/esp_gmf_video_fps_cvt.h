/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_element.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Initializes the GMF video frame rate convert
 *
 * @param[in]   config  No need to set
 * @param[out]  handle  Pointer to the video frame rate convert handle to be initialized
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 *       - ESP_GMF_ERR_MEMORY_LACK  Failed to allocate memory
 */
esp_gmf_err_t esp_gmf_video_fps_cvt_init(void *config, esp_gmf_element_handle_t *handle);

/**
 * @brief  Set GMF video frame rate converter output frame rate
 *
 * @note  This API should only called before element running
 *
 * @param[in]   handle  Video frame rate converter handle
 * @param[out]  fps     Destination frame rate to set (unit frame per second)
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 */
esp_gmf_err_t esp_gmf_video_fps_cvt_set_fps(esp_gmf_element_handle_t handle, uint16_t fps);

#ifdef __cplusplus
}
#endif
