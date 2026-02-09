/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_method.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Define a GMF method identifier
 *
 *         This macro defines a string identifier for a GMF method,
 *         making the code more readable and understandable. The identifier is composed of
 *         three parts:
 *           1. Category: The method's category (e.g., VIDEO)
 *           2. Module: The module to which the method belongs (e.g., ENCODER)
 *           3. Method: The method name itself (e.g., SET_DST_CODEC)
 *
 *         Example usage:
 *           ESP_GMF_METHOD_DEF(VIDEO, ENCODER, SET_DST_CODEC, "set_dst_codec")
 */
#define ESP_GMF_METHOD_DEF(category, module, method, str) \
    static const char* const ESP_GMF_METHOD_##category##_##module##_##method = str

/**
 * @brief  Retrieve the string representation of a method defined with `ESP_GMF_METHOD_DEF`.
 */
#define ESP_GMF_METHOD_STR(category, module, method) \
    (ESP_GMF_METHOD_##category##_##module##_##method)

/**
 * @brief  Define a GMF method argument identifier
 *
 *         This macro defines a string identifier for a method argument, improving code readability.
 *         The identifier is composed of four parts:
 *           1. Category: The method's category (e.g., VIDEO)
 *           2. Module: The module to which the method belongs (e.g., ENCODER)
 *           3. Method: The method name (e.g., SET_DST_CODEC)
 *           4. Argument: The argument name (e.g., CODEC)
 *
 *         Example usage:
 *           ESP_GMF_METHOD_ARG_DEF(VIDEO, ENCODER, SET_DST_CODEC, CODEC, "codec")
 */
#define ESP_GMF_METHOD_ARG_DEF(category, module, method, arg, str) \
    static const char* const ESP_GMF_METHOD_##category##_##module##_##method##_##arg = str

/**
 * @brief  Retrieve the string representation of a method argument defined with `ESP_GMF_METHOD_ARG_DEF`
 */
#define ESP_GMF_METHOD_ARG_STR(category, module, method, arg) \
    (ESP_GMF_METHOD_##category##_##module##_##method##_##arg)

/**
 * @brief  Structure for GMF method execution context
 *         This structure holds the necessary information to execute a GMF method
 *         Including the method pointer and buffer to store execution settings
 */
typedef struct esp_gmf_method_exec_ctx {
    const esp_gmf_method_t *method;    /*!< Pointer to the matched method to be executed */
    uint8_t                *exec_buf;  /*!< Buffer to store executed settings */
    size_t                  buf_size;  /*!< Executed buffer size */
} esp_gmf_method_exec_ctx_t;

/**
 * @brief  Prepare for method execution context by method name
 *
 * @param[in]   method_head  Header of GMF methods
 * @param[in]   method_name  Method name to be searched
 * @param[out]  exec_ctx     Pointer to method execution context
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_NOT_FOUND    Not found method for the method name
 *       - ESP_GMF_ERR_MEMORY_LACK  Not enough memory for method execution buffer
 */
static inline esp_gmf_err_t esp_gmf_method_prepare_exec_ctx(const esp_gmf_method_t *method_head, const char *method_name,
                                                            esp_gmf_method_exec_ctx_t *exec_ctx)
{
    if (method_head) {
        esp_gmf_method_found(method_head, method_name, &exec_ctx->method);
    }
    if (exec_ctx->method == NULL) {
        return ESP_GMF_ERR_NOT_FOUND;
    }
    exec_ctx->exec_buf = NULL;
    // Allocate for execute buffer
    esp_gmf_args_desc_get_total_size(exec_ctx->method->args_desc, &exec_ctx->buf_size);
    if (exec_ctx->buf_size > 0) {
        exec_ctx->exec_buf = esp_gmf_oal_calloc(1, exec_ctx->buf_size);
        if (exec_ctx->exec_buf == NULL) {
            return ESP_GMF_ERR_MEMORY_LACK;
        }
    }
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Release for method execution context
 *
 * @param[in]  exec_ctx  Pointer to method execution context
 */
static inline void esp_gmf_method_release_exec_ctx(esp_gmf_method_exec_ctx_t *exec_ctx)
{
    if (exec_ctx && exec_ctx->exec_buf) {
        esp_gmf_oal_free(exec_ctx->exec_buf);
        exec_ctx->exec_buf = NULL;
    }
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
