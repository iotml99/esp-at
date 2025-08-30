/*
 * UART Utilities for AT Custom Commands
 * 
 * Provides thread-safe UART communication utilities for ESP32 AT commands
 */

#ifndef UART_UTILS_H
#define UART_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize UART utilities
 * 
 * Sets up semaphores and other resources needed for thread-safe UART operations.
 * Must be called before using any other UART utility functions.
 * 
 * @return true on success, false on failure
 */
bool uart_utils_init(void);

/**
 * @brief Thread-safe write to AT UART port
 * 
 * Writes data to the AT UART port with mutex protection to ensure
 * thread-safe operation when multiple tasks need to send data.
 * 
 * @param data Pointer to data buffer to write
 * @param len Number of bytes to write
 */
void at_uart_write_locked(const uint8_t *data, size_t len);

/**
 * @brief Get the UART lock semaphore handle
 * 
 * Returns the semaphore handle used for UART locking.
 * Useful for external modules that need direct semaphore access.
 * 
 * @return SemaphoreHandle_t UART lock semaphore handle, or NULL if not initialized
 */
SemaphoreHandle_t uart_utils_get_lock(void);

/**
 * @brief Get the data input semaphore handle
 * 
 * Returns the semaphore handle used for data input signaling.
 * Useful for external modules that need direct semaphore access.
 * 
 * @return SemaphoreHandle_t Data input semaphore handle, or NULL if not initialized
 */
SemaphoreHandle_t uart_utils_get_data_sema(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_UTILS_H */
