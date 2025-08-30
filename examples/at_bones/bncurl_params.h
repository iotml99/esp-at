/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCURL_PARAMS_H
#define BNCURL_PARAMS_H

#include <stdint.h>
#include <stdbool.h>
#include "bncurl_config.h"

/**
 * @brief BNCURL Parameters Structure
 * 
 * This structure contains all parsed parameters from AT+BNCURL command
 * using fixed-size buffers that persist after AT command buffer is freed.
 */
typedef struct {
    char method[BNCURL_MAX_METHOD_LENGTH + 1];                           // HTTP method (GET, POST, HEAD)
    char url[BNCURL_MAX_URL_LENGTH + 1];                                 // Target URL
    char headers[BNCURL_MAX_HEADERS_COUNT][BNCURL_MAX_HEADER_LENGTH + 1]; // HTTP headers array
    int header_count;                                                    // Number of headers
    char data_upload[BNCURL_MAX_PARAMETER_LENGTH + 1];                   // "-du" parameter (number or @file)
    char data_download[BNCURL_MAX_FILE_PATH_LENGTH + 1];                 // "-dd" parameter (file path)
    char cookie_save[BNCURL_MAX_COOKIE_FILE_PATH + 1];                   // "-c" parameter (cookie file to save)
    char cookie_send[BNCURL_MAX_COOKIE_FILE_PATH + 1];                   // "-b" parameter (cookie file to send)
    char range[BNCURL_MAX_RANGE_STRING_LENGTH + 1];                      // "-r" parameter (range_start-range_end)
    bool verbose;                                                        // "-v" flag
} bncurl_params_t;

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
uint8_t bncurl_parse_and_print_params(uint8_t para_num, bncurl_params_t *params);

#endif // BNCURL_PARAMS_H
