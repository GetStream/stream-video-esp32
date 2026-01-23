/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief  Get audio codec type by uri
 *
 * @param[in]   uri        URI of audio codec
 * @param[out]  format_id  Type of audio codec(ESP_FourCC type)
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_NOT_SUPPORT  Not supported audio codec type
 */
esp_gmf_err_t esp_gmf_audio_helper_get_audio_type_by_uri(const char *uri, uint32_t *format_id);

#ifdef __cplusplus
}
#endif /* __cplusplus */
