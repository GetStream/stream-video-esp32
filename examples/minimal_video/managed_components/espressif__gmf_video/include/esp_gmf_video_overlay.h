/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_element.h"
#include "esp_gmf_video_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Initializes the GMF overlay mixer with the provided configuration
 *
 * @note  This module mixes the overlay plane (foreground) into the original plane (background) using alpha blending
 *        The formula is:
 *        Output_Plane_Pixel = Original_Plane_Pixel * (255 - alpha) + Overlay_Plane_Pixel * alpha
 *        where `alpha` is the transparency level (0 = fully transparent, 255 = fully opaque)
 *
 *        Diagram:
 *        +----------------------------+
 *        |      Original Plane        |
 *        |       (Background)         |
 *        |   +---------------------+  |
 *        |   |   Overlay Plane     |  |
 *        |   |   (Foreground)      |  |
 *        |   +---------------------+  |
 *        +----------------------------+
 *
 * @param[in]   config  No need to set
 * @param[out]  handle  Pointer to the overlay mixer handle to be initialized
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 *       - ESP_GMF_ERR_MEMORY_LACK  Failed to allocate memory
 */
esp_gmf_err_t esp_gmf_video_overlay_init(void *config, esp_gmf_element_handle_t *handle);

/**
 * @brief  Set the output region for video overlay
 *
 * @note  This API should only called before element running
 *
 * @param[in]   handle    Video overlay element handle
 * @param[out]  rgn_info  Region information
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 *       - ESP_GMF_ERR_MEMORY_LACK  Failed to allocate memory
 */
esp_gmf_err_t esp_gmf_video_overlay_set_rgn(esp_gmf_element_handle_t handle, esp_gmf_overlay_rgn_info_t *rgn_info);

/**
 * @brief  Set the GMF port for video overlay input
 *
 * @note  The video overlay element will use this port to acquire overlay data,
 *        mix it into the original video plane, and then release the port
 *        The port is managed by the user, the overlay element only accesses it
 *        This API can be called before element running and allow reset
 *
 * @param[in]  handle  Video overlay element handle
 * @param[in]  port    GMF port for video overlay data
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 *       - ESP_GMF_ERR_MEMORY_LACK  Failed to allocate memory
 */
esp_gmf_err_t esp_gmf_video_overlay_set_overlay_port(esp_gmf_element_handle_t handle, esp_gmf_port_handle_t port);

/**
 * @brief  Set the alpha value of the overlay window
 *
 * @note  Allow to set both before and after element running
 *
 * @param[in]  handle  Video overlay element handle
 * @param[in]  alpha   Overlay window alpha setting:
 *                     0 means fully transparent, 255 means fully opaque
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 */
esp_gmf_err_t esp_gmf_video_overlay_set_alpha(esp_gmf_element_handle_t handle, uint8_t alpha);

/**
 * @brief  Enable overlay mixer or not
 *
 * @note  When overlay mixer is disabled will not acquire data from overlay port and do mixer anymore
 *        Original plane data leave unchanged
 *        Allow to set both before and after element running
 *
 * @param[in]  handle  Video overlay mixer handle
 * @param[in]  enable  Enable mixer or not
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 */
esp_gmf_err_t esp_gmf_video_overlay_enable(esp_gmf_element_handle_t handle, bool enable);

#ifdef __cplusplus
}
#endif /* __cplusplus */
