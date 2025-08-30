/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "esp_at.h"
#include "bncurl_params.h"

// Function to print parsed parameters
static void print_bncurl_params(const bncurl_params_t *params)
{
    printf("=== BNCURL Parameters ===\n");
    printf("Method: %s\n", strlen(params->method) > 0 ? params->method : "NULL");
    printf("URL: %s\n", strlen(params->url) > 0 ? params->url : "NULL");
    
    if (params->header_count > 0) {
        printf("Headers (%d):\n", params->header_count);
        for (int i = 0; i < params->header_count; i++) {
            printf("  [%d]: %s\n", i, params->headers[i]);
        }
    } else {
        printf("Headers: None\n");
    }
    
    printf("Data Upload (-du): %s\n", strlen(params->data_upload) > 0 ? params->data_upload : "None");
    printf("Data Download (-dd): %s\n", strlen(params->data_download) > 0 ? params->data_download : "None");
    printf("Cookie Save (-c): %s\n", strlen(params->cookie_save) > 0 ? params->cookie_save : "None");
    printf("Cookie Send (-b): %s\n", strlen(params->cookie_send) > 0 ? params->cookie_send : "None");
    printf("Range (-r): %s\n", strlen(params->range) > 0 ? params->range : "None");
    printf("Verbose (-v): %s\n", params->verbose ? "Yes" : "No");
    printf("========================\n");
}

// Function to initialize parameters structure
static void init_bncurl_params(bncurl_params_t *params)
{
    memset(params, 0, sizeof(bncurl_params_t));
}

// Function to validate method
static bool is_valid_method(const char *method)
{
    if (!method) return false;
    
    // Valid methods: GET, POST, HEAD (case sensitive)
    return (strcmp(method, "GET") == 0 || 
            strcmp(method, "POST") == 0 || 
            strcmp(method, "HEAD") == 0);
}

// Function to validate URL
static bool is_valid_url(const char *url)
{
    if (!url || strlen(url) == 0) return false;
    
    // Must start with http:// or https://
    return (strncmp(url, "http://", 7) == 0 || 
            strncmp(url, "https://", 8) == 0);
}

// Function to validate parameter combinations
static bool validate_param_combinations(const bncurl_params_t *params)
{
    // GET/HEAD cannot have data upload
    if ((strcmp(params->method, "GET") == 0 || strcmp(params->method, "HEAD") == 0) && 
        strlen(params->data_upload) > 0) {
        printf("ERROR: GET/HEAD methods cannot have data upload (-du)\n");
        return false;
    }
    
    // POST/HEAD cannot have range
    if ((strcmp(params->method, "POST") == 0 || strcmp(params->method, "HEAD") == 0) && 
        strlen(params->range) > 0) {
        printf("ERROR: POST/HEAD methods cannot have range (-r)\n");
        return false;
    }
    
    // Range requires data download
    if (strlen(params->range) > 0 && strlen(params->data_download) == 0) {
        printf("ERROR: Range (-r) requires data download (-dd)\n");
        return false;
    }
    
    // POST requires data upload
    if (strcmp(params->method, "POST") == 0 && strlen(params->data_upload) == 0) {
        printf("ERROR: POST method requires data upload (-du)\n");
        return false;
    }
    
    return true;
}

// Main parsing function
static uint8_t parse_bncurl_params(uint8_t para_num)
{
    bncurl_params_t params;
    uint8_t index = 0;
    uint8_t *param_str = NULL;
    
    // Initialize parameters structure
    init_bncurl_params(&params);
    
    printf("Parsing BNCURL command with %d parameters\n", para_num);
    
    // Need at least 2 parameters: method and URL
    if (para_num < 2) {
        printf("ERROR: Insufficient parameters. Need at least method and URL\n");
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Parse method (first parameter)
    if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
        printf("ERROR: Failed to parse method parameter\n");
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Copy method with length validation
    if (strlen((char *)param_str) > BNCURL_MAX_METHOD_LENGTH) {
        printf("ERROR: Method too long. Max length: %d\n", BNCURL_MAX_METHOD_LENGTH);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    strncpy(params.method, (char *)param_str, BNCURL_MAX_METHOD_LENGTH);
    params.method[BNCURL_MAX_METHOD_LENGTH] = '\0';
    
    if (!is_valid_method(params.method)) {
        printf("ERROR: Invalid method '%s'. Valid methods: GET, POST, HEAD\n", params.method);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Parse URL (second parameter)
    if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
        printf("ERROR: Failed to parse URL parameter\n");
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Copy URL with length validation
    if (strlen((char *)param_str) > BNCURL_MAX_URL_LENGTH) {
        printf("ERROR: URL too long. Max length: %d\n", BNCURL_MAX_URL_LENGTH);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    strncpy(params.url, (char *)param_str, BNCURL_MAX_URL_LENGTH);
    params.url[BNCURL_MAX_URL_LENGTH] = '\0';
    
    if (!is_valid_url(params.url)) {
        printf("ERROR: Invalid URL '%s'. Must start with http:// or https://\n", params.url);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Parse optional parameters
    while (index < para_num) {
        if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
            printf("ERROR: Failed to parse parameter at index %d\n", index - 1);
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        char *option = (char *)param_str;
        
        if (strcmp(option, "-H") == 0) {
            // Header option - next parameter is the header value
            if (index >= para_num) {
                printf("ERROR: -H option requires a header value\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (params.header_count >= BNCURL_MAX_HEADERS_COUNT) {
                printf("ERROR: Too many headers. Max allowed: %d\n", BNCURL_MAX_HEADERS_COUNT);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
                printf("ERROR: Failed to parse header value\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            // Copy header with length validation
            if (strlen((char *)param_str) > BNCURL_MAX_HEADER_LENGTH) {
                printf("ERROR: Header too long. Max length: %d\n", BNCURL_MAX_HEADER_LENGTH);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            strncpy(params.headers[params.header_count], (char *)param_str, BNCURL_MAX_HEADER_LENGTH);
            params.headers[params.header_count][BNCURL_MAX_HEADER_LENGTH] = '\0';
            params.header_count++;
            
        } else if (strcmp(option, "-du") == 0) {
            // Data upload option
            if (strlen(params.data_upload) > 0) {
                printf("ERROR: Duplicate -du option\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (index >= para_num) {
                printf("ERROR: -du option requires a value\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
                printf("ERROR: Failed to parse -du value\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            // Copy data upload parameter with length validation
            if (strlen((char *)param_str) > BNCURL_MAX_PARAMETER_LENGTH) {
                printf("ERROR: Data upload parameter too long. Max length: %d\n", BNCURL_MAX_PARAMETER_LENGTH);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            strncpy(params.data_upload, (char *)param_str, BNCURL_MAX_PARAMETER_LENGTH);
            params.data_upload[BNCURL_MAX_PARAMETER_LENGTH] = '\0';
            
        } else if (strcmp(option, "-dd") == 0) {
            // Data download option
            if (strlen(params.data_download) > 0) {
                printf("ERROR: Duplicate -dd option\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (index >= para_num) {
                printf("ERROR: -dd option requires a file path\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
                printf("ERROR: Failed to parse -dd value\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            char *path_str = (char *)param_str;
            
            // Handle @ prefix normalization
            if (path_str[0] == '@') {
                path_str++; // Skip the @ character
                printf("INFO: Normalized -dd path (removed @ prefix)\n");
            }
            
            // Copy data download path with length validation
            if (strlen(path_str) > BNCURL_MAX_FILE_PATH_LENGTH) {
                printf("ERROR: File path too long. Max length: %d\n", BNCURL_MAX_FILE_PATH_LENGTH);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            strncpy(params.data_download, path_str, BNCURL_MAX_FILE_PATH_LENGTH);
            params.data_download[BNCURL_MAX_FILE_PATH_LENGTH] = '\0';
            
        } else if (strcmp(option, "-c") == 0) {
            // Cookie save option
            if (strlen(params.cookie_save) > 0) {
                printf("ERROR: Duplicate -c option\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (index >= para_num) {
                printf("ERROR: -c option requires a cookie file path\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
                printf("ERROR: Failed to parse -c value\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            // Copy cookie save path with length validation
            if (strlen((char *)param_str) > BNCURL_MAX_COOKIE_FILE_PATH) {
                printf("ERROR: Cookie file path too long. Max length: %d\n", BNCURL_MAX_COOKIE_FILE_PATH);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            strncpy(params.cookie_save, (char *)param_str, BNCURL_MAX_COOKIE_FILE_PATH);
            params.cookie_save[BNCURL_MAX_COOKIE_FILE_PATH] = '\0';
            
        } else if (strcmp(option, "-b") == 0) {
            // Cookie send option
            if (strlen(params.cookie_send) > 0) {
                printf("ERROR: Duplicate -b option\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (index >= para_num) {
                printf("ERROR: -b option requires a cookie file path\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
                printf("ERROR: Failed to parse -b value\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            // Copy cookie send path with length validation
            if (strlen((char *)param_str) > BNCURL_MAX_COOKIE_FILE_PATH) {
                printf("ERROR: Cookie file path too long. Max length: %d\n", BNCURL_MAX_COOKIE_FILE_PATH);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            strncpy(params.cookie_send, (char *)param_str, BNCURL_MAX_COOKIE_FILE_PATH);
            params.cookie_send[BNCURL_MAX_COOKIE_FILE_PATH] = '\0';
            
        } else if (strcmp(option, "-r") == 0) {
            // Range option
            if (strlen(params.range) > 0) {
                printf("ERROR: Duplicate -r option\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (index >= para_num) {
                printf("ERROR: -r option requires a range value\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
                printf("ERROR: Failed to parse -r value\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            // Copy range parameter with length validation
            if (strlen((char *)param_str) > BNCURL_MAX_RANGE_STRING_LENGTH) {
                printf("ERROR: Range parameter too long. Max length: %d\n", BNCURL_MAX_RANGE_STRING_LENGTH);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            strncpy(params.range, (char *)param_str, BNCURL_MAX_RANGE_STRING_LENGTH);
            params.range[BNCURL_MAX_RANGE_STRING_LENGTH] = '\0';
            
        } else if (strcmp(option, "-v") == 0) {
            // Verbose option (no additional parameter)
            params.verbose = true;
            
        } else {
            printf("ERROR: Unknown option '%s'\n", option);
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    
    // Validate parameter combinations
    if (!validate_param_combinations(&params)) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Print the parsed parameters
    print_bncurl_params(&params);
    
    return ESP_AT_RESULT_CODE_OK;
}

// Export the parsing function for use in at_bones.c
uint8_t bncurl_parse_and_print_params(uint8_t para_num)
{
    return parse_bncurl_params(para_num);
}
