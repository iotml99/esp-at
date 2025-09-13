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
    
    // Additional fields for numeric data upload
    bool is_numeric_upload;                                              // True if -du is a number (not @file)
    size_t upload_bytes_expected;                                        // Number of bytes to collect from UART
    char *collected_data;                                                // Buffer for collected UART data
    size_t collected_data_size;                                          // Actual size of collected data
} bncurl_params_t;

/**
 * @brief Parse and print BNCURL command parameters
 * 
 * This function parses the parameters of an AT+BNCURL command and prints
 * them in a structured format. It validates the parameter combinations
 * according to the BNCURL specification and performs comprehensive file
 * system validation.
 * 
 * Supported parameters:
 * - method: GET, POST, HEAD
 * - url: HTTP/HTTPS URL
 * - -H: HTTP headers (can appear multiple times)
 * - -du: Data upload (number of bytes or @file path with mount point substitution)
 * - -dd: Data download (file path with @ prefix mount point substitution)
 * - -c: Cookie save file (with @ prefix mount point substitution)
 * - -b: Cookie send file (with @ prefix mount point substitution)
 * - -r: Range (start-end)
 * - -v: Verbose mode flag
 * 
 * Path Normalization:
 * - Paths starting with "@/" are converted to "/mount_point/"
 * - Paths starting with "@" are converted to "/mount_point/"
 * - Example: "@Downloads" → "/sdcard/Downloads"
 * - Example: "@/Downloads" → "/sdcard/Downloads"
 * 
 * File System Validation:
 * - SD card mount status is checked when @ paths are used
 * - Directories are created recursively if they don't exist
 * - Download files: directories created, existing files will be overwritten
 * - Upload/cookie files: validated to exist and be readable
 * - Disk space is validated before file operations
 * - Returns error if SD card not mounted when @ paths specified
 * 
 * Error Conditions:
 * - SD card not mounted when using @ file paths
 * - Cannot create required directories
 * - Insufficient disk space for downloads
 * - Upload/cookie files don't exist or aren't readable
 * - File path length exceeds maximum limits
 * 
 * @param para_num Number of parameters in the AT command
 * @param params Pointer to bncurl_params_t structure to fill
 * @return ESP_AT_RESULT_CODE_OK on success, ESP_AT_RESULT_CODE_ERROR on failure
 */
uint8_t bncurl_parse_and_print_params(uint8_t para_num, bncurl_params_t *params);

/**
 * @brief Clean up allocated resources in bncurl_params_t structure
 * 
 * This function frees any dynamically allocated memory in the parameters
 * structure, particularly the collected_data buffer used for numeric uploads.
 * 
 * @param params Pointer to bncurl_params_t structure to clean up
 */
void bncurl_params_cleanup(bncurl_params_t *params);



/**
 * @brief Get the configured URL from BNURLCFG command
 * 
 * This function returns the URL that was set using the AT+BNURLCFG command.
 * This URL can be used as a substitute when "." is passed as the URL parameter.
 * 
 * @return Pointer to the configured URL string, or NULL if no URL is configured
 */
const char* bncurl_get_configured_url(void);

/**
 * @brief Set the configured URL for BNURLCFG command
 * 
 * This function is used internally to store the URL from AT+BNURLCFG command.
 * 
 * @param url The URL to store (must be valid HTTP/HTTPS URL)
 * @return true on success, false on error
 */
bool bncurl_set_configured_url(const char* url);

#endif // BNCURL_PARAMS_H
