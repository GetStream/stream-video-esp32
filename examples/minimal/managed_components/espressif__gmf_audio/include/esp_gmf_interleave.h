/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"
#include "esp_gmf_element.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DEFAULT_ESP_GMF_INTERLEAVE_CONFIG() {  \
    .sample_rate     = 48000,                  \
    .bits_per_sample = 16,                     \
    .src_num         = 2,                      \
}

/**
 * @brief  Configuration structure for interleave
 */
typedef struct {
    uint32_t sample_rate;     /*!< The audio sample rate */
    uint8_t  bits_per_sample; /*!< The audio bits per sample, supports 16, 24, 32 bits */
    uint8_t  src_num;         /*!< The number of input source num */
} esp_gmf_interleave_cfg;

/**
 * @brief  Initializes the GMF interleave with the provided configuration
 *
 * @param[in]   config  Pointer to the interleave configuration
 * @param[out]  handle  Pointer to the interleave handle to be initialized
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 *       - ESP_GMF_ERR_MEMORY_LACK  Failed to allocate memory
 */
esp_gmf_err_t esp_gmf_interleave_init(esp_gmf_interleave_cfg *config, esp_gmf_element_handle_t *handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */
