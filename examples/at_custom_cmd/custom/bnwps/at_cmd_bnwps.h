/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BNWPS error code mapping (used in +CWJAP:<code> responses)
 * 
 * These codes are emitted as +CWJAP:<code> followed by ERROR on failure paths
 */
typedef enum {
    BNWPS_ERR_GENERAL_FAILURE = 1,      ///< General failure
    BNWPS_ERR_TIMEOUT = 2,              ///< Timeout
    BNWPS_ERR_WPS_FAILED = 3,           ///< WPS failed (protocol)
    BNWPS_ERR_INVALID_ARGS = 4,         ///< Invalid arguments
    BNWPS_ERR_NOT_INITIALIZED = 5,      ///< Not initialized / Wi-Fi off
    BNWPS_ERR_BUSY = 6,                 ///< Busy / operation in progress
    BNWPS_ERR_CANCELED = 7,             ///< Canceled by user
    BNWPS_ERR_AUTH_FAILED = 8,          ///< Auth failed
    BNWPS_ERR_NOT_SUPPORTED = 9,        ///< Feature not supported on this target/build
} bnwps_error_code_t;

/**
 * @brief Initialize the BNWPS subsystem
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bnwps_init(void);

/**
 * @brief Deinitialize the BNWPS subsystem
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bnwps_deinit(void);

/**
 * @brief Register BNWPS AT commands
 * 
 * @return true on success, false on failure
 */
bool at_cmd_bnwps_register(void);

/* AT command handlers */
uint8_t at_bnwps_cmd_test(uint8_t *cmd_name);
uint8_t at_bnwps_cmd_query(uint8_t *cmd_name);
uint8_t at_bnwps_cmd_setup(uint8_t para_num);

#ifdef __cplusplus
}
#endif
