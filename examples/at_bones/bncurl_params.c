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

typedef struct {
    char *method;
    char *url;
    char **headers;
    int header_count;
    char *data_upload;      // "-du" parameter (number or @file)
    char *data_download;    // "-dd" parameter (file path)
    char *cookie_save;      // "-c" parameter (cookie file to save)
    char *cookie_send;      // "-b" parameter (cookie file to send)
    char *range;            // "-r" parameter (range_start-range_end)
    bool verbose;           // "-v" flag
} bncurl_params_t;

// Function to print parsed parameters
static void print_bncurl_params(const bncurl_params_t *params)
{
    printf("=== BNCURL Parameters ===\n");
    printf("Method: %s\n", params->method ? params->method : "NULL");
    printf("URL: %s\n", params->url ? params->url : "NULL");
    
    if (params->header_count > 0) {
        printf("Headers (%d):\n", params->header_count);
        for (int i = 0; i < params->header_count; i++) {
            printf("  [%d]: %s\n", i, params->headers[i]);
        }
    } else {
        printf("Headers: None\n");
    }
    
    printf("Data Upload (-du): %s\n", params->data_upload ? params->data_upload : "None");
    printf("Data Download (-dd): %s\n", params->data_download ? params->data_download : "None");
    printf("Cookie Save (-c): %s\n", params->cookie_save ? params->cookie_save : "None");
    printf("Cookie Send (-b): %s\n", params->cookie_send ? params->cookie_send : "None");
    printf("Range (-r): %s\n", params->range ? params->range : "None");
    printf("Verbose (-v): %s\n", params->verbose ? "Yes" : "No");
    printf("========================\n");
}

// Function to free allocated memory
static void free_bncurl_params(bncurl_params_t *params)
{
    if (params->method) {
        free(params->method);
    }
    if (params->url) {
        free(params->url);
    }
    if (params->headers) {
        for (int i = 0; i < params->header_count; i++) {
            if (params->headers[i]) {
                free(params->headers[i]);
            }
        }
        free(params->headers);
    }
    if (params->data_upload) {
        free(params->data_upload);
    }
    if (params->data_download) {
        free(params->data_download);
    }
    if (params->cookie_save) {
        free(params->cookie_save);
    }
    if (params->cookie_send) {
        free(params->cookie_send);
    }
    if (params->range) {
        free(params->range);
    }
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
        params->data_upload) {
        printf("ERROR: GET/HEAD methods cannot have data upload (-du)\n");
        return false;
    }
    
    // POST/HEAD cannot have range
    if ((strcmp(params->method, "POST") == 0 || strcmp(params->method, "HEAD") == 0) && 
        params->range) {
        printf("ERROR: POST/HEAD methods cannot have range (-r)\n");
        return false;
    }
    
    // Range requires data download
    if (params->range && !params->data_download) {
        printf("ERROR: Range (-r) requires data download (-dd)\n");
        return false;
    }
    
    // POST requires data upload
    if (strcmp(params->method, "POST") == 0 && !params->data_upload) {
        printf("ERROR: POST method requires data upload (-du)\n");
        return false;
    }
    
    return true;
}

// Main parsing function
static uint8_t parse_bncurl_params(uint8_t para_num)
{
    bncurl_params_t params = {0};
    uint8_t index = 0;
    uint8_t *param_str = NULL;
    
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
    params.method = strdup((char *)param_str);
    
    if (!is_valid_method(params.method)) {
        printf("ERROR: Invalid method '%s'. Valid methods: GET, POST, HEAD\n", params.method);
        free_bncurl_params(&params);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Parse URL (second parameter)
    if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
        printf("ERROR: Failed to parse URL parameter\n");
        free_bncurl_params(&params);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    params.url = strdup((char *)param_str);
    
    if (!is_valid_url(params.url)) {
        printf("ERROR: Invalid URL '%s'. Must start with http:// or https://\n", params.url);
        free_bncurl_params(&params);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Parse optional parameters
    while (index < para_num) {
        if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
            printf("ERROR: Failed to parse parameter at index %d\n", index - 1);
            free_bncurl_params(&params);
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        char *option = (char *)param_str;
        
        if (strcmp(option, "-H") == 0) {
            // Header option - next parameter is the header value
            if (index >= para_num) {
                printf("ERROR: -H option requires a header value\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
                printf("ERROR: Failed to parse header value\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            // Allocate or reallocate headers array
            params.headers = realloc(params.headers, (params.header_count + 1) * sizeof(char*));
            if (!params.headers) {
                printf("ERROR: Memory allocation failed for headers\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            params.headers[params.header_count] = strdup((char *)param_str);
            params.header_count++;
            
        } else if (strcmp(option, "-du") == 0) {
            // Data upload option
            if (params.data_upload) {
                printf("ERROR: Duplicate -du option\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (index >= para_num) {
                printf("ERROR: -du option requires a value\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
                printf("ERROR: Failed to parse -du value\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            params.data_upload = strdup((char *)param_str);
            
        } else if (strcmp(option, "-dd") == 0) {
            // Data download option
            if (params.data_download) {
                printf("ERROR: Duplicate -dd option\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (index >= para_num) {
                printf("ERROR: -dd option requires a file path\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
                printf("ERROR: Failed to parse -dd value\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            params.data_download = strdup((char *)param_str);
            
            // Handle @ prefix normalization
            if (params.data_download[0] == '@') {
                char *normalized = strdup(params.data_download + 1);
                free(params.data_download);
                params.data_download = normalized;
                printf("INFO: Normalized -dd path (removed @ prefix)\n");
            }
            
        } else if (strcmp(option, "-c") == 0) {
            // Cookie save option
            if (params.cookie_save) {
                printf("ERROR: Duplicate -c option\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (index >= para_num) {
                printf("ERROR: -c option requires a cookie file path\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
                printf("ERROR: Failed to parse -c value\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            params.cookie_save = strdup((char *)param_str);
            
        } else if (strcmp(option, "-b") == 0) {
            // Cookie send option
            if (params.cookie_send) {
                printf("ERROR: Duplicate -b option\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (index >= para_num) {
                printf("ERROR: -b option requires a cookie file path\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
                printf("ERROR: Failed to parse -b value\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            params.cookie_send = strdup((char *)param_str);
            
        } else if (strcmp(option, "-r") == 0) {
            // Range option
            if (params.range) {
                printf("ERROR: Duplicate -r option\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (index >= para_num) {
                printf("ERROR: -r option requires a range value\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
                printf("ERROR: Failed to parse -r value\n");
                free_bncurl_params(&params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            params.range = strdup((char *)param_str);
            
        } else if (strcmp(option, "-v") == 0) {
            // Verbose option (no additional parameter)
            params.verbose = true;
            
        } else {
            printf("ERROR: Unknown option '%s'\n", option);
            free_bncurl_params(&params);
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    
    // Validate parameter combinations
    if (!validate_param_combinations(&params)) {
        free_bncurl_params(&params);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Print the parsed parameters
    print_bncurl_params(&params);
    
    // Free allocated memory
    free_bncurl_params(&params);
    
    return ESP_AT_RESULT_CODE_OK;
}

// Export the parsing function for use in at_bones.c
uint8_t bncurl_parse_and_print_params(uint8_t para_num)
{
    return parse_bncurl_params(para_num);
}
