/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCERT_H
#define BNCERT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_at.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum certificate file path length
 */
#define BNCERT_MAX_FILE_PATH_LENGTH 256

/**
 * @brief Maximum certificate data size that can be flashed at once
 */
#define BNCERT_MAX_DATA_SIZE (4 * 1024)

/**
 * @brief Certificate data source type
 */
typedef enum {
    BNCERT_SOURCE_FILE = 0,     ///< Data source is a file on SD card (starts with @)
    BNCERT_SOURCE_UART = 1      ///< Data source is UART input (numeric byte count)
} bncert_data_source_t;

/**
 * @brief Certificate flashing parameters
 */
typedef struct {
    uint32_t flash_address;             ///< Absolute flash memory address
    bncert_data_source_t source_type;   ///< Data source type (file or UART)
    char file_path[BNCERT_MAX_FILE_PATH_LENGTH + 1]; ///< File path (if source is file)
    size_t data_size;                   ///< Data size in bytes (if source is UART)
    uint8_t *uart_data;                 ///< Buffer for UART data collection
    size_t collected_size;              ///< Actually collected data size
} bncert_params_t;

/**
 * @brief Certificate flashing result
 */
typedef enum {
    BNCERT_RESULT_OK = 0,           ///< Operation successful
    BNCERT_RESULT_INVALID_PARAMS,   ///< Invalid parameters
    BNCERT_RESULT_FILE_ERROR,       ///< File operation error
    BNCERT_RESULT_FLASH_ERROR,      ///< Flash operation error
    BNCERT_RESULT_MEMORY_ERROR,     ///< Memory allocation error
    BNCERT_RESULT_UART_ERROR        ///< UART data collection error
} bncert_result_t;

/**
 * @brief Initialize certificate flashing subsystem
 * 
 * Sets up the necessary resources for certificate flashing operations.
 * Must be called before using any other certificate functions.
 * 
 * @return true on success, false on failure
 */
bool bncert_init(void);

/**
 * @brief Deinitialize certificate flashing subsystem
 * 
 * Cleans up resources used by the certificate flashing subsystem.
 */
void bncert_deinit(void);

/**
 * @brief Parse certificate flashing command parameters
 * 
 * Parses the AT+BNFLASH_CERT command parameters and validates them.
 * 
 * @param para_num Number of parameters
 * @param params Pointer to parameters structure to fill
 * @return ESP_AT_RESULT_CODE_OK on success, ESP_AT_RESULT_CODE_ERROR on failure
 */
uint8_t bncert_parse_params(uint8_t para_num, bncert_params_t *params);

/**
 * @brief Execute certificate flashing operation
 * 
 * Flashes certificate data to the specified flash address from either
 * a file on SD card or data received via UART.
 * 
 * @param params Certificate flashing parameters
 * @return Certificate flashing result
 */
bncert_result_t bncert_flash_certificate(bncert_params_t *params);

/**
 * @brief Collect data from UART for certificate flashing
 * 
 * Collects the specified number of bytes from UART after sending
 * the '>' prompt to the user.
 * 
 * @param params Certificate parameters with data_size specified
 * @return true if data collected successfully, false on error
 */
bool bncert_collect_uart_data(bncert_params_t *params);

/**
 * @brief Validate flash address for certificate storage
 * 
 * Checks if the specified flash address is valid and safe for
 * certificate storage (not in bootloader, partition table, or app areas).
 * 
 * @param address Flash address to validate
 * @param size Data size to be written
 * @return true if address is valid, false otherwise
 */
bool bncert_validate_flash_address(uint32_t address, size_t size);

/**
 * @brief Clean up certificate parameters
 * 
 * Frees any dynamically allocated memory in the parameters structure.
 * 
 * @param params Certificate parameters to clean up
 */
void bncert_cleanup_params(bncert_params_t *params);

/**
 * @brief Get result description string
 * 
 * Returns a human-readable description of the certificate operation result.
 * 
 * @param result Certificate operation result
 * @return Pointer to result description string
 */
const char* bncert_get_result_string(bncert_result_t result);

#ifdef __cplusplus
}
#endif

#endif // BNCERT_H
