/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bn_constants.h
 * @brief Common constants for AT Bones module
 * 
 * This file contains all shared constants used across the AT Bones module
 * to avoid magic numbers and improve code maintainability.
 */

// ============================================================================
// Buffer Size Constants
// ============================================================================

/**
 * @brief Small buffer size for simple status responses
 * 
 * Use for:
 * - Single digit status codes (e.g., "+BNWPS:0")
 * - Boolean responses
 * - Simple acknowledgments
 * - Mount point names
 * - Chunk headers
 * - Marker buffers
 */
#define BN_BUFFER_SMALL         32

/**
 * @brief Medium buffer size for short responses
 * 
 * Use for:
 * - Error messages
 * - Timeout values
 * - Single-field responses
 * - Short status messages
 * - Length markers
 * - Size information strings
 */
#define BN_BUFFER_MEDIUM        64

/**
 * @brief Standard buffer size for typical responses
 * 
 * Use for:
 * - Multi-field status responses
 * - Standard AT command responses
 * - Progress indicators
 * - Mount point information
 * - Range headers
 */
#define BN_BUFFER_STANDARD      128

/**
 * @brief Large buffer size for extended responses
 * 
 * Use for:
 * - URL configuration responses
 * - File path information
 * - Multi-line status output
 * - Longer text responses
 * - Response buffers
 * - Normalized paths
 * - Cookie lines
 * - Range headers (complex)
 */
#define BN_BUFFER_LARGE         256

/**
 * @brief Extended buffer size for detailed information
 * 
 * Use for:
 * - Help text and command documentation
 * - Detailed error messages
 * - Multi-paragraph responses
 * - SD card space info with uint64 strings
 * - URL query responses
 * - Cookie strings
 * - Header lines
 */
#define BN_BUFFER_EXTENDED      512

#ifdef __cplusplus
}
#endif
