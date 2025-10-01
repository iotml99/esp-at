/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CERT_BUNDLE_H
#define CERT_BUNDLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Simplified Certificate Bundle System
 * 
 * This system uses a single 256KB partition for certificate bundle storage:
 * - Partition Layout: [4 bytes length][4 bytes CRC32][up to 255KB bundle data]
 * - Bundle data is stored as PEM format (ready for TLS use)
 * - Validation on startup with fallback to hardcoded bundle
 * - Direct flash access for zero-RAM usage
 * - AT commands for bundle management
 */

/**
 * @brief Certificate bundle partition layout
 */
typedef struct {
    uint32_t bundle_length;    ///< Bundle size in bytes (0 = no bundle)
    uint32_t bundle_crc32;     ///< CRC32 of bundle data
    // Bundle data follows immediately after this header
} cert_bundle_header_t;

/**
 * @brief Bundle validation status
 */
typedef enum {
    CERT_BUNDLE_STATUS_NONE = 0,        ///< No bundle stored
    CERT_BUNDLE_STATUS_VALID = 1,       ///< Valid bundle available
    CERT_BUNDLE_STATUS_CORRUPTED = 2    ///< Bundle exists but corrupted
} cert_bundle_status_t;

/**
 * @brief Bundle operation results
 */
typedef enum {
    CERT_BUNDLE_OK = 0,                 ///< Operation successful
    CERT_BUNDLE_ERROR_INVALID_PARAM,    ///< Invalid parameters
    CERT_BUNDLE_ERROR_PARTITION,        ///< Partition access error
    CERT_BUNDLE_ERROR_MEMORY,           ///< Memory allocation error
    CERT_BUNDLE_ERROR_TOO_LARGE,        ///< Bundle size exceeds limit
    CERT_BUNDLE_ERROR_CRC,              ///< CRC validation failed
    CERT_BUNDLE_ERROR_WRITE,            ///< Flash write error
    CERT_BUNDLE_ERROR_SEMAPHORE,        ///< Semaphore error
    CERT_BUNDLE_ERROR_UART              ///< UART data collection error
} cert_bundle_result_t;

/**
 * @brief Bundle information structure
 */
typedef struct {
    cert_bundle_status_t status;        ///< Bundle validation status
    uint32_t bundle_size;               ///< Bundle size in bytes
    uint32_t bundle_crc32;              ///< Bundle CRC32
} cert_bundle_info_t;

/**
 * @brief Data source for bundle flashing
 */
typedef enum {
    CERT_BUNDLE_SOURCE_SD = 0,          ///< Load from SD card file
    CERT_BUNDLE_SOURCE_UART = 1         ///< Load from UART input
} cert_bundle_source_t;

/**
 * @brief Bundle flashing context for UART operations
 */
typedef struct {
    SemaphoreHandle_t uart_semaphore;   ///< UART access semaphore
    uint8_t buffer_a[1024];             ///< Buffer A for ping-pong operation
    uint8_t buffer_b[1024];             ///< Buffer B for ping-pong operation
    uint8_t *read_buffer;               ///< Current UART read buffer
    uint8_t *write_buffer;              ///< Current flash write buffer
    size_t read_size;                   ///< Bytes in read buffer
    size_t write_size;                  ///< Bytes in write buffer
    size_t total_received;              ///< Total bytes received
    size_t expected_size;               ///< Expected total size
    bool uart_active;                   ///< UART collection active
} cert_bundle_flash_context_t;

// Partition configuration
#define CERT_BUNDLE_PARTITION_SUBTYPE   0x40        ///< Certificate partition subtype
#define CERT_BUNDLE_HEADER_SIZE         8           ///< Header size (length + CRC)
#define CERT_BUNDLE_MAX_SIZE            (256 * 1024 - CERT_BUNDLE_HEADER_SIZE)  ///< Max bundle data size (255KB)

/**
 * @brief Initialize certificate bundle system
 * 
 * Finds the certificate partition, validates any existing bundle,
 * and sets up the system for bundle operations.
 * 
 * @param hardcoded_bundle Pointer to hardcoded CA bundle (fallback)
 * @param hardcoded_size Size of hardcoded bundle
 * @return true on success, false on failure
 */
bool cert_bundle_init(const char *hardcoded_bundle, size_t hardcoded_size);

/**
 * @brief Deinitialize certificate bundle system
 * 
 * Cleans up resources and frees memory.
 */
void cert_bundle_deinit(void);

/**
 * @brief Get certificate bundle for TLS use
 * 
 * Returns a pointer to the certificate bundle (flashed or hardcoded).
 * This is the main interface for TLS integration.
 * 
 * @param bundle_ptr Pointer to store bundle data pointer
 * @param bundle_size Pointer to store bundle size
 * @return CERT_BUNDLE_OK on success, error code on failure
 */
cert_bundle_result_t cert_bundle_get(const char **bundle_ptr, size_t *bundle_size);

/**
 * @brief Get bundle information and status
 * 
 * Returns detailed information about the current bundle state.
 * 
 * @param info Pointer to store bundle information
 * @return CERT_BUNDLE_OK on success, error code on failure
 */
cert_bundle_result_t cert_bundle_get_info(cert_bundle_info_t *info);

/**
 * @brief Flash certificate bundle from SD card
 * 
 * Loads and flashes a certificate bundle from a file on SD card.
 * 
 * @param file_path Path to certificate bundle file (PEM format)
 * @return CERT_BUNDLE_OK on success, error code on failure
 */
cert_bundle_result_t cert_bundle_flash_from_sd(const char *file_path);

/**
 * @brief Flash certificate bundle from UART
 * 
 * Collects certificate bundle data from UART and flashes it to partition.
 * Uses dual-buffer system with semaphore protection.
 * 
 * @param bundle_size Expected bundle size in bytes
 * @return CERT_BUNDLE_OK on success, error code on failure
 */
cert_bundle_result_t cert_bundle_flash_from_uart(size_t bundle_size);

/**
 * @brief Clear certificate bundle partition
 * 
 * Erases the certificate partition, forcing fallback to hardcoded bundle.
 * 
 * @return CERT_BUNDLE_OK on success, error code on failure
 */
cert_bundle_result_t cert_bundle_clear(void);

/**
 * @brief Validate PEM certificate bundle format
 * 
 * Performs basic validation of PEM certificate bundle format.
 * 
 * @param bundle_data Bundle data to validate
 * @param bundle_size Bundle size in bytes
 * @return true if valid PEM format, false otherwise
 */
bool cert_bundle_validate_pem(const uint8_t *bundle_data, size_t bundle_size);

/**
 * @brief Convert result code to string
 * 
 * @param result Bundle operation result
 * @return Human-readable description
 */
const char* cert_bundle_result_to_string(cert_bundle_result_t result);

// AT Command Interface Functions

/**
 * @brief Handle AT+BNCERT_FLASH command
 * 
 * Parses parameters and initiates bundle flashing operation.
 * Syntax: AT+BNCERT_FLASH=<source>,<param>
 * - source=0, param=file_path: Flash from SD card
 * - source=1, param=size: Flash from UART
 * 
 * @param para_num Number of parameters
 * @return ESP_AT_RESULT_CODE_OK or ESP_AT_RESULT_CODE_ERROR
 */
uint8_t at_bncert_flash_cmd(uint8_t para_num);

/**
 * @brief Handle AT+BNCERT_CLEAR command
 * 
 * Clears the certificate bundle partition.
 * Syntax: AT+BNCERT_CLEAR
 * 
 * @param cmd_name Command name (exe function signature)
 * @return ESP_AT_RESULT_CODE_OK or ESP_AT_RESULT_CODE_ERROR
 */
uint8_t at_bncert_clear_cmd(uint8_t *cmd_name);

/**
 * @brief Handle AT+BNCERT? command
 * 
 * Returns certificate bundle status and information.
 * Syntax: AT+BNCERT?
 * Response: +BNCERT:<status>,<size>,<crc32>
 * 
 * @param para_num Number of parameters (should be 0)
 * @return ESP_AT_RESULT_CODE_OK or ESP_AT_RESULT_CODE_ERROR
 */
uint8_t at_bncert_query_cmd(uint8_t para_num);

#ifdef __cplusplus
}
#endif

#endif // CERT_BUNDLE_H