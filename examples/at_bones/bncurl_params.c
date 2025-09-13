/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <unistd.h>
#include <dirent.h>
#include "esp_at.h"
#include "esp_log.h"
#include "bncurl_params.h"
#include "bnsd.h"

static const char *TAG = "BNCURL_PARAMS";

// Global storage for configured URL from BNURLCFG command
static char s_configured_url[BNCURL_MAX_URL_LENGTH + 1] = {0};

// Function to get configured URL
const char* bncurl_get_configured_url(void)
{
    if (strlen(s_configured_url) > 0) {
        return s_configured_url;
    }
    return NULL;
}

// Function to set configured URL
bool bncurl_set_configured_url(const char* url)
{
    if (!url) {
        // Clear the configured URL
        s_configured_url[0] = '\0';
        return true;
    }
    
    // Validate URL length
    if (strlen(url) > BNCURL_MAX_URL_LENGTH) {
        return false;
    }
    
    // Validate URL format
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        return false;
    }
    
    // Store the URL
    strncpy(s_configured_url, url, BNCURL_MAX_URL_LENGTH);
    s_configured_url[BNCURL_MAX_URL_LENGTH] = '\0';
    
    return true;
}

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

// Function to validate that file path starts with @ prefix for SD card
static bool validate_file_path_prefix(const char *file_path, const char *param_name)
{
    if (!file_path || strlen(file_path) == 0) {
        return true; // No file path specified, nothing to validate
    }
    
    if (file_path[0] != '@') {
        ESP_LOGE(TAG, "Invalid file path for %s: %s (must start with @)", param_name, file_path);
        printf("ERROR: File path for %s must start with @ (SD card prefix): %s\n", param_name, file_path);
        return false;
    }
    
    return true;
}

// Function to validate file exists for reading (cookie send files)
static bool validate_file_exists_for_reading(const char *file_path)
{
    if (!file_path || strlen(file_path) == 0) {
        return true; // No file path specified, nothing to validate
    }
    
    struct stat file_stat;
    if (stat(file_path, &file_stat) != 0) {
        ESP_LOGE(TAG, "File does not exist for reading: %s", file_path);
        printf("ERROR: File does not exist: %s\n", file_path);
        return false;
    }
    
    if (!S_ISREG(file_stat.st_mode)) {
        ESP_LOGE(TAG, "Path is not a regular file: %s", file_path);
        printf("ERROR: Path is not a file: %s\n", file_path);
        return false;
    }
    
    // Test if file can be opened for reading
    FILE *test_fp = fopen(file_path, "r");
    if (!test_fp) {
        ESP_LOGE(TAG, "Cannot open file for reading: %s", file_path);
        printf("ERROR: Cannot open file for reading: %s\n", file_path);
        return false;
    }
    
    fclose(test_fp);
    ESP_LOGI(TAG, "File validation successful for reading: %s", file_path);
    return true;
}

// Function to validate and prepare file path for download
static bool validate_and_prepare_download_path(const char *file_path)
{
    if (!file_path || strlen(file_path) == 0) {
        return true; // No file path specified, nothing to validate
    }
    
    // Extract directory path from file path
    char dir_path[BNCURL_MAX_FILE_PATH_LENGTH + 1];
    strncpy(dir_path, file_path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
    
    // Find the last '/' to separate directory from filename
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL) {
        *last_slash = '\0'; // Terminate string at last slash to get directory path
        
        // Use bnsd_mkdir_recursive to create directory
        if (!bnsd_mkdir_recursive(dir_path)) {
            ESP_LOGE(TAG, "Failed to create directory for file: %s", file_path);
            printf("ERROR: Failed to create directory for file: %s\n", file_path);
            return false;
        }
    }
    
    // Check if file already exists (it will be overwritten)
    struct stat file_stat;
    if (stat(file_path, &file_stat) == 0) {
        ESP_LOGI(TAG, "File %s already exists and will be overwritten", file_path);
        printf("INFO: File %s exists and will be overwritten\n", file_path);
    }
    
    // Check disk space by trying to create a test file
    char test_file[BNCURL_MAX_FILE_PATH_LENGTH + 20];
    snprintf(test_file, sizeof(test_file), "%s.tmp_space_test", file_path);
    
    FILE *test_fp = fopen(test_file, "w");
    if (!test_fp) {
        ESP_LOGE(TAG, "Cannot create file %s: %s", file_path, strerror(errno));
        printf("ERROR: Cannot create file %s: insufficient disk space or permission denied\n", file_path);
        return false;
    }
    
    fclose(test_fp);
    unlink(test_file); // Remove test file
    
    ESP_LOGI(TAG, "File path validation successful: %s", file_path);
    return true;
}

// Function to validate SD card paths and prepare file operations
static bool validate_and_prepare_sd_file_operations(const bncurl_params_t *params)
{
    bool has_sd_file_paths = false;
    
    // Check if any SD card file paths are specified
    if (strlen(params->data_download) > 0 && strncmp(params->data_download, BNSD_MOUNT_POINT, strlen(BNSD_MOUNT_POINT)) == 0) {
        has_sd_file_paths = true;
    }
    if (strlen(params->data_upload) > 0 && strncmp(params->data_upload, BNSD_MOUNT_POINT, strlen(BNSD_MOUNT_POINT)) == 0) {
        has_sd_file_paths = true;
    }
    if (strlen(params->cookie_save) > 0 && strncmp(params->cookie_save, BNSD_MOUNT_POINT, strlen(BNSD_MOUNT_POINT)) == 0) {
        has_sd_file_paths = true;
    }
    if (strlen(params->cookie_send) > 0 && strncmp(params->cookie_send, BNSD_MOUNT_POINT, strlen(BNSD_MOUNT_POINT)) == 0) {
        has_sd_file_paths = true;
    }
    
    // If SD card file paths are specified, SD card must be mounted
    if (has_sd_file_paths) {
        if (!bnsd_is_mounted()) {
            ESP_LOGE(TAG, "SD card is not mounted but file paths are specified");
            printf("ERROR: SD card must be mounted to use @ file paths\n");
            return false;
        }
        ESP_LOGI(TAG, "SD card validation passed for file operations");
    } else {
        // No SD card paths, validation passed
        return true;
    }
    
    // Validate and prepare download file path if specified
    if (strlen(params->data_download) > 0 && strncmp(params->data_download, BNSD_MOUNT_POINT, strlen(BNSD_MOUNT_POINT)) == 0) {
        if (!validate_and_prepare_download_path(params->data_download)) {
            return false;
        }
    }
    
    // Validate and prepare cookie save file path if specified
    if (strlen(params->cookie_save) > 0 && strncmp(params->cookie_save, BNSD_MOUNT_POINT, strlen(BNSD_MOUNT_POINT)) == 0) {
        if (!validate_and_prepare_download_path(params->cookie_save)) {
            return false;
        }
    }
    
    // Validate cookie send file exists if specified
    if (strlen(params->cookie_send) > 0 && strncmp(params->cookie_send, BNSD_MOUNT_POINT, strlen(BNSD_MOUNT_POINT)) == 0) {
        if (!validate_file_exists_for_reading(params->cookie_send)) {
            return false;
        }
    }
    
    // Validate data upload file exists if it's a file path (starts with mount point)
    if (strlen(params->data_upload) > 0 && strncmp(params->data_upload, BNSD_MOUNT_POINT, strlen(BNSD_MOUNT_POINT)) == 0) {
        if (!validate_file_exists_for_reading(params->data_upload)) {
            return false;
        }
    }
    
    return true;
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
    
    // Range is supported for both file download (-dd) and UART streaming
    // No additional validation needed - range works with both modes
    
    // POST can optionally have data upload, but it's not required for empty POST
    // Empty POST requests (without -du) are valid and will send no body
    // This explicitly allows: AT+BNCURL="POST","https://httpbin.org/post"
    if (strcmp(params->method, "POST") == 0) {
        ESP_LOGI(TAG, "POST method validated - data upload is optional");
        printf("INFO: POST method validated - data upload (-du) is optional\n");
    }
    
    return true;
}

// Main parsing function
static uint8_t parse_bncurl_params(uint8_t para_num, bncurl_params_t *params)
{
    uint8_t index = 0;
    uint8_t *param_str = NULL;
    
    // Initialize parameters structure
    init_bncurl_params(params);
    
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
    strncpy(params->method, (char *)param_str, BNCURL_MAX_METHOD_LENGTH);
    params->method[BNCURL_MAX_METHOD_LENGTH] = '\0';
    
    if (!is_valid_method(params->method)) {
        printf("ERROR: Invalid method '%s'. Valid methods: GET, POST, HEAD\n", params->method);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Parse URL (second parameter)
    if (esp_at_get_para_as_str(index++, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
        printf("ERROR: Failed to parse URL parameter\n");
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Check if URL is "." - substitute with configured URL
    if (strcmp((char *)param_str, ".") == 0) {
        const char* configured_url = bncurl_get_configured_url();
        if (!configured_url) {
            printf("ERROR: No URL configured with AT+BNURLCFG. Cannot use '.' as URL.\n");
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        // Use the configured URL
        strncpy(params->url, configured_url, BNCURL_MAX_URL_LENGTH);
        params->url[BNCURL_MAX_URL_LENGTH] = '\0';
        
        printf("INFO: Using configured URL: %s\n", params->url);
        ESP_LOGI(TAG, "Substituted '.' with configured URL: %s", params->url);
    } else {
        // Copy URL with length validation
        if (strlen((char *)param_str) > BNCURL_MAX_URL_LENGTH) {
            printf("ERROR: URL too long. Max length: %d\n", BNCURL_MAX_URL_LENGTH);
            return ESP_AT_RESULT_CODE_ERROR;
        }
        strncpy(params->url, (char *)param_str, BNCURL_MAX_URL_LENGTH);
        params->url[BNCURL_MAX_URL_LENGTH] = '\0';
    }
    
    if (!is_valid_url(params->url)) {
        printf("ERROR: Invalid URL '%s'. Must start with http:// or https://\n", params->url);
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
            
            if (params->header_count >= BNCURL_MAX_HEADERS_COUNT) {
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
            
            strncpy(params->headers[params->header_count], (char *)param_str, BNCURL_MAX_HEADER_LENGTH);
            params->headers[params->header_count][BNCURL_MAX_HEADER_LENGTH] = '\0';
            params->header_count++;
            
        } else if (strcmp(option, "-du") == 0) {
            // Data upload option
            if (strlen(params->data_upload) > 0) {
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
            
            char *upload_str = (char *)param_str;
            
            // Copy data upload parameter with length validation
            if (strlen(upload_str) > BNCURL_MAX_PARAMETER_LENGTH) {
                printf("ERROR: Data upload parameter too long. Max length: %d\n", BNCURL_MAX_PARAMETER_LENGTH);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            strncpy(params->data_upload, upload_str, BNCURL_MAX_PARAMETER_LENGTH);
            params->data_upload[BNCURL_MAX_PARAMETER_LENGTH] = '\0';
            
            // Check if this is a numeric value (for UART input) or file path
            if (upload_str[0] != '@') {
                // Check if it's a valid number
                char *endptr;
                long bytes = strtol(upload_str, &endptr, 10);
                
                if (*endptr == '\0' && bytes >= 0 && bytes <= 65536) {
                    // Valid numeric value - will collect data from UART
                    params->is_numeric_upload = true;
                    params->upload_bytes_expected = (size_t)bytes;
                    ESP_LOGI(TAG, "Numeric upload detected: %u bytes expected from UART", (unsigned int)params->upload_bytes_expected);
                    
                    // Special case: 0 bytes is valid for empty POST
                    if (params->upload_bytes_expected == 0) {
                        printf("INFO: Will send empty POST data (0 bytes)\n");
                    } else {
                        printf("INFO: Will collect %u bytes from UART after OK\n", (unsigned int)params->upload_bytes_expected);
                    }
                } else {
                    // Not a valid number and doesn't start with @ - invalid
                    printf("ERROR: Invalid -du value: %s (must be numeric 0-65536 or file path starting with @)\n", upload_str);
                    return ESP_AT_RESULT_CODE_ERROR;
                }
            } else {
                // File path - validate @ prefix (already has it, but validate anyway for consistency)
                if (!validate_file_path_prefix(upload_str, "-du")) {
                    return ESP_AT_RESULT_CODE_ERROR;
                }
                
                // File path - normalize it
                params->is_numeric_upload = false;
                bnsd_normalize_path_with_mount_point(params->data_upload, BNCURL_MAX_PARAMETER_LENGTH);
            }
            
        } else if (strcmp(option, "-dd") == 0) {
            // Data download option
            if (strlen(params->data_download) > 0) {
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
            
            // Validate that file path starts with @ prefix
            if (!validate_file_path_prefix(path_str, "-dd")) {
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            // Copy data download path with length validation
            if (strlen(path_str) > BNCURL_MAX_FILE_PATH_LENGTH) {
                printf("ERROR: File path too long. Max length: %d\n", BNCURL_MAX_FILE_PATH_LENGTH);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            strncpy(params->data_download, path_str, BNCURL_MAX_FILE_PATH_LENGTH);
            params->data_download[BNCURL_MAX_FILE_PATH_LENGTH] = '\0';
            
            // Normalize path (remove @ and prepend mount point)
            bnsd_normalize_path_with_mount_point(params->data_download, BNCURL_MAX_FILE_PATH_LENGTH);
            
        } else if (strcmp(option, "-c") == 0) {
            // Cookie save option
            if (strlen(params->cookie_save) > 0) {
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
            
            // Validate that cookie save path starts with @ prefix
            if (!validate_file_path_prefix((char *)param_str, "-c")) {
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            // Copy cookie save path with length validation
            if (strlen((char *)param_str) > BNCURL_MAX_COOKIE_FILE_PATH) {
                printf("ERROR: Cookie file path too long. Max length: %d\n", BNCURL_MAX_COOKIE_FILE_PATH);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            strncpy(params->cookie_save, (char *)param_str, BNCURL_MAX_COOKIE_FILE_PATH);
            params->cookie_save[BNCURL_MAX_COOKIE_FILE_PATH] = '\0';
            
            // Normalize path (remove @ and prepend mount point)
            bnsd_normalize_path_with_mount_point(params->cookie_save, BNCURL_MAX_COOKIE_FILE_PATH);
            
        } else if (strcmp(option, "-b") == 0) {
            // Cookie send option
            if (strlen(params->cookie_send) > 0) {
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
            
            // Validate that cookie send path starts with @ prefix
            if (!validate_file_path_prefix((char *)param_str, "-b")) {
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            // Copy cookie send path with length validation
            if (strlen((char *)param_str) > BNCURL_MAX_COOKIE_FILE_PATH) {
                printf("ERROR: Cookie file path too long. Max length: %d\n", BNCURL_MAX_COOKIE_FILE_PATH);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            strncpy(params->cookie_send, (char *)param_str, BNCURL_MAX_COOKIE_FILE_PATH);
            params->cookie_send[BNCURL_MAX_COOKIE_FILE_PATH] = '\0';
            
            // Normalize path (remove @ and prepend mount point)
            bnsd_normalize_path_with_mount_point(params->cookie_send, BNCURL_MAX_COOKIE_FILE_PATH);
            
        } else if (strcmp(option, "-r") == 0) {
            // Range option
            if (strlen(params->range) > 0) {
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
            strncpy(params->range, (char *)param_str, BNCURL_MAX_RANGE_STRING_LENGTH);
            params->range[BNCURL_MAX_RANGE_STRING_LENGTH] = '\0';
            
            // Basic range format validation (should be "start-end")
            char *dash = strchr(params->range, '-');
            if (!dash || dash == params->range || dash == params->range + strlen(params->range) - 1) {
                printf("ERROR: Invalid range format. Use: start-end (e.g., 0-2097151)\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            // Validate that start and end are numbers
            char *endptr;
            long start = strtol(params->range, &endptr, 10);
            if (endptr != dash || start < 0) {
                printf("ERROR: Invalid range start value. Must be non-negative number\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            long end = strtol(dash + 1, &endptr, 10);
            if (*endptr != '\0' || end < start) {
                printf("ERROR: Invalid range end value. Must be >= start value\n");
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            ESP_LOGI(TAG, "Range validated: %ld-%ld (%ld bytes)", start, end, end - start + 1);
            
        } else if (strcmp(option, "-v") == 0) {
            // Verbose option (no additional parameter)
            params->verbose = true;
            
        } else {
            printf("ERROR: Unknown option '%s'\n", option);
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    
    // Validate parameter combinations
    if (!validate_param_combinations(params)) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Validate SD card file operations (combined check for mount status and file preparation)
    if (!validate_and_prepare_sd_file_operations(params)) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Print the parsed parameters
    print_bncurl_params(params);
    
    return ESP_AT_RESULT_CODE_OK;
}

// Export the parsing function for use in at_bones.c
uint8_t bncurl_parse_and_print_params(uint8_t para_num, bncurl_params_t *params)
{
    return parse_bncurl_params(para_num, params);
}

void bncurl_params_cleanup(bncurl_params_t *params)
{
    if (params == NULL) {
        return;
    }
    
    // Free allocated UART data buffer
    if (params->collected_data != NULL) {
        free(params->collected_data);
        params->collected_data = NULL;
        params->collected_data_size = 0;
    }
    
    // Reset numeric upload fields
    params->is_numeric_upload = false;
    params->upload_bytes_expected = 0;
}
