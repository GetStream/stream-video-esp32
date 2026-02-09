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
 * @brief  Video resolution definition
 */
typedef struct {
    uint16_t  width;   /*!< Width of the video in pixels */
    uint16_t  height;  /*!< Height of the video in pixels */
} esp_gmf_video_resolution_t;

/**
 * @brief  Video region definition
 *         Represents a rectangular region of the video frame
 */
typedef struct {
    uint16_t  x;       /*!< X-coordinate of the top-left corner */
    uint16_t  y;       /*!< Y-coordinate of the top-left corner */
    uint16_t  width;   /*!< Width of the region in pixels */
    uint16_t  height;  /*!< Height of the region in pixels */
} esp_gmf_video_rgn_t;

/**
 * @brief  Specifies the format and destination region for a video overlay
 *
 * @note   This structure defines the parameters needed to position and format an overlay on a video frame
 */
typedef struct {
    uint32_t             format_id;  /*!< FourCC representation of overlay format */
    esp_gmf_video_rgn_t  dst_rgn;    /*!< Region position to put the overlay */
} esp_gmf_overlay_rgn_info_t;

#ifdef __cplusplus
}
#endif
