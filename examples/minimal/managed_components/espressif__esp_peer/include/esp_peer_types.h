/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Peer error code
 */
typedef enum {
    ESP_PEER_ERR_NONE         = 0,  /*!< None error */
    ESP_PEER_ERR_INVALID_ARG  = -1, /*!< Invalid argument */
    ESP_PEER_ERR_NO_MEM       = -2, /*!< Not enough memory */
    ESP_PEER_ERR_WRONG_STATE  = -3, /*!< Operate on wrong state */
    ESP_PEER_ERR_NOT_SUPPORT  = -4, /*!< Not supported operation */
    ESP_PEER_ERR_NOT_EXISTS   = -5, /*!< Not existed */
    ESP_PEER_ERR_FAIL         = -6, /*!< General error code */
    ESP_PEER_ERR_OVER_LIMITED = -7, /*!< Overlimited */
    ESP_PEER_ERR_BAD_DATA     = -8, /*!< Bad input data */
    ESP_PEER_ERR_WOULD_BLOCK  = -9, /*!< Not enough buffer for output packet, need sleep and retry later */
} esp_peer_err_t;

/**
 * @brief  ICE server configuration
 */
typedef struct {
    char  *stun_url; /*!< STUN/Relay server URL */
    char  *user;     /*!< User name */
    char  *psw;      /*!< User password */
} esp_peer_ice_server_cfg_t;

/**
 * @brief  Peer role
 */
typedef enum {
    ESP_PEER_ROLE_CONTROLLING, /*!< Controlling role who initialize the connection */
    ESP_PEER_ROLE_CONTROLLED,  /*!< Controlled role */
} esp_peer_role_t;

#ifdef __cplusplus
}
#endif
