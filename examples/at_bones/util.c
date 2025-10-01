/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "util.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "esp_at.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "UTIL";

// UART data collection timeout (30 seconds)
#define UART_DATA_COLLECTION_TIMEOUT_MS 30000

// Semaphore for UART data collection synchronization
static SemaphoreHandle_t s_uart_data_sync_sema = NULL;

// Callback for UART data collection
static void uart_data_wait_callback(void)
{
    if (s_uart_data_sync_sema) {
        xSemaphoreGive(s_uart_data_sync_sema);
    }
}

/**
 * @brief Convert uint64_t to character array (string representation)
 */
int uint64_to_string(uint64_t value, char *buffer, size_t buffer_size)
{
    // Input validation
    if (!buffer || buffer_size == 0) {
        return -1;
    }
    
    // Handle special case of zero
    if (value == 0) {
        if (buffer_size < 2) { // Need space for '0' + null terminator
            return -1;
        }
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }
    
    // Calculate number of digits needed
    // Maximum uint64_t value is 18446744073709551615 (20 digits)
    char temp_buffer[21]; // 20 digits + null terminator
    int digit_count = 0;
    uint64_t temp_value = value;
    
    // Extract digits in reverse order
    while (temp_value > 0) {
        temp_buffer[digit_count] = (char)('0' + (temp_value % 10));
        temp_value /= 10;
        digit_count++;
    }
    
    // Check if output buffer is large enough
    if (buffer_size < (size_t)(digit_count + 1)) {
        return -1;
    }
    
    // Reverse the digits into the output buffer
    for (int i = 0; i < digit_count; i++) {
        buffer[i] = temp_buffer[digit_count - 1 - i];
    }
    
    // Add null terminator
    buffer[digit_count] = '\0';
    
    return digit_count;
}

/**
 * @brief Convert uint64_t to hexadecimal character array (string representation)
 */
int uint64_to_hex_string(uint64_t value, char *buffer, size_t buffer_size, bool uppercase)
{
    // Input validation
    if (!buffer || buffer_size == 0) {
        return -1;
    }
    
    // Handle special case of zero
    if (value == 0) {
        if (buffer_size < 2) { // Need space for '0' + null terminator
            return -1;
        }
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }
    
    // Calculate number of hex digits needed
    // Maximum uint64_t value is 0xFFFFFFFFFFFFFFFF (16 hex digits)
    char temp_buffer[17]; // 16 hex digits + null terminator
    int digit_count = 0;
    uint64_t temp_value = value;
    
    // Define hex characters
    const char *hex_chars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    
    // Extract hex digits in reverse order
    while (temp_value > 0) {
        temp_buffer[digit_count] = hex_chars[temp_value & 0xF];
        temp_value >>= 4;
        digit_count++;
    }
    
    // Check if output buffer is large enough
    if (buffer_size < (size_t)(digit_count + 1)) {
        return -1;
    }
    
    // Reverse the digits into the output buffer
    for (int i = 0; i < digit_count; i++) {
        buffer[i] = temp_buffer[digit_count - 1 - i];
    }
    
    // Add null terminator
    buffer[digit_count] = '\0';
    
    return digit_count;
}

/**
 * @brief Helper function to validate converted string (for testing purposes)
 * 
 * This function can be used to verify that the conversion was successful
 * by checking basic properties of the result string.
 */
bool validate_uint64_string(const char *str, bool is_hex)
{
    if (!str || str[0] == '\0') {
        return false;
    }
    
    // Check each character
    for (const char *p = str; *p != '\0'; p++) {
        if (is_hex) {
            // Valid hex characters: 0-9, a-f, A-F
            if (!((*p >= '0' && *p <= '9') || 
                  (*p >= 'a' && *p <= 'f') || 
                  (*p >= 'A' && *p <= 'F'))) {
                return false;
            }
        } else {
            // Valid decimal characters: 0-9
            if (*p < '0' || *p > '9') {
                return false;
            }
        }
    }
    
    return true;
}

/**
 * @brief Collect data from UART with timeout
 */
bool collect_uart_data(size_t expected_bytes, char **collected_data, size_t *collected_size)
{
    if (expected_bytes == 0) {
        // Special case: 0 bytes - no data collection needed
        *collected_data = NULL;
        *collected_size = 0;
        ESP_LOGI(TAG, "No UART data collection needed (0 bytes expected)");
        return true;
    }
    
    // Allocate buffer for collected data
    *collected_data = malloc(expected_bytes + 1); // +1 for null terminator if needed
    if (!*collected_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for UART data collection");
        return false;
    }
    
    // Create semaphore for data collection synchronization
    if (!s_uart_data_sync_sema) {
        s_uart_data_sync_sema = xSemaphoreCreateBinary();
        if (!s_uart_data_sync_sema) {
            ESP_LOGE(TAG, "Failed to create UART data sync semaphore");
            free(*collected_data);
            *collected_data = NULL;
            return false;
        }
    }
    
    *collected_size = 0;
    uint32_t timeout_ticks = pdMS_TO_TICKS(UART_DATA_COLLECTION_TIMEOUT_MS);
    
    ESP_LOGI(TAG, "Collecting %u bytes from UART (timeout: %d ms)", (unsigned int)expected_bytes, UART_DATA_COLLECTION_TIMEOUT_MS);
    
    // Enter specific mode for UART data collection
    esp_at_port_enter_specific(uart_data_wait_callback);
    
    // Show prompt
    esp_at_port_write_data((uint8_t *)">", 1);
    
    // Collect data using ESP-AT framework
    while (*collected_size < expected_bytes) {
        if (xSemaphoreTake(s_uart_data_sync_sema, timeout_ticks) == pdTRUE) {
            // Read available data
            size_t bytes_to_read = expected_bytes - *collected_size;
            size_t bytes_read = esp_at_port_read_data((uint8_t *)(*collected_data + *collected_size), bytes_to_read);
            *collected_size += bytes_read;
            
            ESP_LOGD(TAG, "Read %u bytes, total collected: %u/%u", 
                     (unsigned int)bytes_read, (unsigned int)*collected_size, (unsigned int)expected_bytes);
            
            if (*collected_size >= expected_bytes) {
                break;
            }
        } else {
            // Timeout occurred
            ESP_LOGW(TAG, "UART data collection timeout after %d ms", UART_DATA_COLLECTION_TIMEOUT_MS);
            ESP_LOGE(TAG, "Timeout waiting for %u bytes (collected %u)", (unsigned int)expected_bytes, (unsigned int)*collected_size);
            esp_at_port_exit_specific();
            vSemaphoreDelete(s_uart_data_sync_sema);
            s_uart_data_sync_sema = NULL;
            free(*collected_data);
            *collected_data = NULL;
            return false;
        }
    }
    
    // Exit specific mode
    esp_at_port_exit_specific();
    
    // Clean up semaphore
    vSemaphoreDelete(s_uart_data_sync_sema);
    s_uart_data_sync_sema = NULL;
    
    // Null-terminate for safety (doesn't count toward data size)
    (*collected_data)[*collected_size] = '\0';
    
    ESP_LOGI(TAG, "Successfully collected %u bytes from UART", (unsigned int)*collected_size);
    return true;
}
