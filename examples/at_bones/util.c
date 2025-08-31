/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "util.h"
#include <string.h>
#include <stdbool.h>

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
