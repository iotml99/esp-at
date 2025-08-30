/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCURL_PARAMS_H
#define BNCURL_PARAMS_H

#include <stdint.h>

/**
 * @brief Parse and print BNCURL command parameters
 * 
 * This function parses the parameters of an AT+BNCURL command and prints
 * them in a structured format. It validates the parameter combinations
 * according to the BNCURL specification.
 * 
 * Supported parameters:
 * - method: GET, POST, HEAD
 * - url: HTTP/HTTPS URL
 * - -H: HTTP headers (can appear multiple times)
 * - -du: Data upload (number of bytes or @file)
 * - -dd: Data download (file path, @ prefix normalized)
 * - -c: Cookie save file
 * - -b: Cookie send file  
 * - -r: Range (start-end)
 * - -v: Verbose mode flag
 * 
 * @param para_num Number of parameters in the AT command
 * @return ESP_AT_RESULT_CODE_OK on success, ESP_AT_RESULT_CODE_ERROR on failure
 */
uint8_t bncurl_parse_and_print_params(uint8_t para_num);

#endif // BNCURL_PARAMS_H
