/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Convert uint64_t to character array (string representation)
 * 
 * This function converts a 64-bit unsigned integer to its decimal string representation.
 * The function is designed to be safe and efficient for use in embedded systems.
 * 
 * @param value The uint64_t value to convert
 * @param buffer Pointer to the character array where the result will be stored
 * @param buffer_size Size of the buffer (must be at least 21 bytes for uint64_t max value)
 * 
 * @return int Number of characters written to buffer (excluding null terminator),
 *             or -1 if buffer is too small or parameters are invalid
 * 
 * @note The buffer must be at least 21 bytes to accommodate the maximum uint64_t value
 *       (18446744073709551615) plus null terminator. For safety, recommend 32 bytes.
 * 
 * @example
 * char str[32];
 * int len = uint64_to_string(12345678901234567890ULL, str, sizeof(str));
 * if (len > 0) {
 *     printf("Converted: %s (length: %d)\n", str, len);
 * }
 */
int uint64_to_string(uint64_t value, char *buffer, size_t buffer_size);

/**
 * @brief Convert uint64_t to hexadecimal character array (string representation)
 * 
 * This function converts a 64-bit unsigned integer to its hexadecimal string representation.
 * The output is in lowercase format (e.g., "1a2b3c4d5e6f7890").
 * 
 * @param value The uint64_t value to convert
 * @param buffer Pointer to the character array where the result will be stored
 * @param buffer_size Size of the buffer (must be at least 17 bytes for uint64_t max value)
 * @param uppercase If true, use uppercase letters (A-F), if false use lowercase (a-f)
 * 
 * @return int Number of characters written to buffer (excluding null terminator),
 *             or -1 if buffer is too small or parameters are invalid
 * 
 * @note The buffer must be at least 17 bytes to accommodate the maximum uint64_t value
 *       (16 hex digits) plus null terminator. For safety, recommend 32 bytes.
 * 
 * @example
 * char hex_str[32];
 * int len = uint64_to_hex_string(0x1A2B3C4D5E6F7890ULL, hex_str, sizeof(hex_str), true);
 * if (len > 0) {
 *     printf("Hex: %s (length: %d)\n", hex_str, len);
 * }
 */
int uint64_to_hex_string(uint64_t value, char *buffer, size_t buffer_size, bool uppercase);

/**
 * @brief Helper function to validate converted string (for testing purposes)
 * 
 * This function can be used to verify that the conversion was successful
 * by checking basic properties of the result string.
 * 
 * @param str The string to validate
 * @param is_hex If true, validate as hexadecimal string, otherwise as decimal
 * 
 * @return true if string is valid, false otherwise
 * 
 * @example
 * char str[32];
 * uint64_to_string(12345, str, sizeof(str));
 * if (validate_uint64_string(str, false)) {
 *     printf("Valid decimal string: %s\n", str);
 * }
 */
bool validate_uint64_string(const char *str, bool is_hex);

#endif /* UTIL_H */