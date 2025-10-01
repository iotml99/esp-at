/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cert_bundle.h"
#include "esp_at.h"
#include "esp_log.h"
#include "bnsd.h"
#include <string.h>

// External hardcoded CA bundle
extern const char CA_BUNDLE_PEM[];

static const char *TAG = "CERT_BUNDLE_AT";

uint8_t at_bncert_flash_cmd(uint8_t para_num)
{
    cert_bundle_result_t result;
    int32_t source_type;
    int32_t param_value;
    uint8_t *param_str = NULL;
    int param_str_len;

    // Parameter validation
    if (para_num != 2) {
        ESP_LOGE(TAG, "AT+BNCERT_FLASH requires exactly 2 parameters");
        return ESP_AT_RESULT_CODE_ERROR;
    }

    // Parse source type (0=SD, 1=UART)
    if (esp_at_get_para_as_digit(0, &source_type) != ESP_AT_PARA_PARSE_RESULT_OK) {
        ESP_LOGE(TAG, "Failed to parse source type parameter");
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (source_type == 0) {
        // Source: SD Card - parameter is file path string
        if (esp_at_get_para_as_str(1, &param_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
            ESP_LOGE(TAG, "Failed to parse file path parameter");
            return ESP_AT_RESULT_CODE_ERROR;
        }

        // Ensure null termination
        param_str_len = strlen((char*)param_str);
        if (param_str_len == 0 || param_str_len > 255) {
            ESP_LOGE(TAG, "Invalid file path length: %d", param_str_len);
            return ESP_AT_RESULT_CODE_ERROR;
        }

        // Create a copy for path normalization (the function modifies in place)
        char normalized_path[256];
        strncpy(normalized_path, (const char*)param_str, sizeof(normalized_path) - 1);
        normalized_path[sizeof(normalized_path) - 1] = '\0';
        
        // Normalize path (handle @ prefix conversion to mount point)
        bnsd_normalize_path_with_mount_point(normalized_path, sizeof(normalized_path));
        
        ESP_LOGI(TAG, "Flashing certificate bundle from SD: %s (normalized: %s)", 
                (char*)param_str, normalized_path);
        result = cert_bundle_flash_from_sd(normalized_path);

    } else if (source_type == 1) {
        // Source: UART - parameter is bundle size
        if (esp_at_get_para_as_digit(1, &param_value) != ESP_AT_PARA_PARSE_RESULT_OK) {
            ESP_LOGE(TAG, "Failed to parse bundle size parameter");
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (param_value <= 0 || param_value > CERT_BUNDLE_MAX_SIZE) {
            ESP_LOGE(TAG, "Invalid bundle size: %d (max %u)", 
                    (int)param_value, CERT_BUNDLE_MAX_SIZE);
            return ESP_AT_RESULT_CODE_ERROR;
        }

        ESP_LOGI(TAG, "Flashing certificate bundle from UART: %d bytes", (int)param_value);
        result = cert_bundle_flash_from_uart((size_t)param_value);

    } else {
        ESP_LOGE(TAG, "Invalid source type: %d (must be 0=SD or 1=UART)", (int)source_type);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    // Check result
    if (result == CERT_BUNDLE_OK) {
        ESP_LOGI(TAG, "Certificate bundle flashed successfully");
        return ESP_AT_RESULT_CODE_OK;
    } else {
        ESP_LOGE(TAG, "Certificate bundle flash failed: %s", cert_bundle_result_to_string(result));
        return ESP_AT_RESULT_CODE_ERROR;
    }
}

uint8_t at_bncert_clear_cmd(uint8_t *cmd_name)
{
    // Immediate output to confirm function is called
    esp_at_port_write_data((uint8_t *)"DEBUG: CLEAR command called\r\n", 29);
    ESP_LOGI(TAG, "AT+BNCERT_CLEAR command called");
    
    // Exe functions don't have parameters - they are called for parameterless commands

    // Try direct clear first - skip initialization check temporarily
    ESP_LOGI(TAG, "Attempting direct certificate bundle clear");
    esp_at_port_write_data((uint8_t *)"DEBUG: Calling cert_bundle_clear\r\n", 34);
    
    cert_bundle_result_t result = cert_bundle_clear();
    ESP_LOGI(TAG, "Clear result: %d", result);
    
    if (result == CERT_BUNDLE_OK) {
        ESP_LOGI(TAG, "Certificate bundle cleared successfully");
        esp_at_port_write_data((uint8_t *)"SUCCESS: Bundle cleared\r\n", 25);
        return ESP_AT_RESULT_CODE_OK;
    } else {
        ESP_LOGE(TAG, "Certificate bundle clear failed: %s", cert_bundle_result_to_string(result));
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "ERROR: Clear failed - %s\r\n", cert_bundle_result_to_string(result));
        esp_at_port_write_data((uint8_t *)error_msg, strlen(error_msg));
        return ESP_AT_RESULT_CODE_ERROR;
    }
}

uint8_t at_bncert_query_cmd(uint8_t para_num)
{
    // No parameters expected for query
    if (para_num != 0) {
        ESP_LOGE(TAG, "AT+BNCERT? expects no parameters");
        return ESP_AT_RESULT_CODE_ERROR;
    }

    cert_bundle_info_t info;
    cert_bundle_result_t result = cert_bundle_get_info(&info);
    
    if (result != CERT_BUNDLE_OK) {
        ESP_LOGE(TAG, "Failed to get certificate bundle info: %s", cert_bundle_result_to_string(result));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    // Format response: +BNCERT:<status>,<size>,<crc32>
    // Status: 0=none, 1=valid, 2=corrupted
    esp_at_port_write_data((uint8_t *)"+BNCERT:", 8);
    
    // Write status
    char status_str[16];
    snprintf(status_str, sizeof(status_str), "%d,", (int)info.status);
    esp_at_port_write_data((uint8_t *)status_str, strlen(status_str));
    
    // Write size
    char size_str[16];
    snprintf(size_str, sizeof(size_str), "%u,", (unsigned int)info.bundle_size);
    esp_at_port_write_data((uint8_t *)size_str, strlen(size_str));
    
    // Write CRC32 (in hex)
    char crc_str[16];
    snprintf(crc_str, sizeof(crc_str), "0x%08X", (unsigned int)info.bundle_crc32);
    esp_at_port_write_data((uint8_t *)crc_str, strlen(crc_str));
    
    // End line
    esp_at_port_write_data((uint8_t *)"\r\n", 2);

    ESP_LOGI(TAG, "Certificate bundle info: status=%d, size=%u, crc=0x%08X", 
            (int)info.status, (unsigned int)info.bundle_size, (unsigned int)info.bundle_crc32);

    return ESP_AT_RESULT_CODE_OK;
}