/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNWPS_H
#define BNWPS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_wifi.h"
#include "esp_wps.h"
#include "esp_at.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum WPS timeout duration in seconds
 */
#define BNWPS_MAX_TIMEOUT_SECONDS 300

/**
 * @brief WPS operation status
 */
typedef enum {
    BNWPS_STATUS_IDLE = 0,      ///< WPS is not active
    BNWPS_STATUS_ACTIVE = 1,    ///< WPS is active and waiting for connection
    BNWPS_STATUS_SUCCESS = 2,   ///< WPS connection succeeded
    BNWPS_STATUS_FAILED = 3,    ///< WPS connection failed
    BNWPS_STATUS_TIMEOUT = 4    ///< WPS operation timed out
} bnwps_status_t;

/**
 * @brief WPS connection result structure
 */
typedef struct {
    char ssid[33];              ///< Connected SSID (null-terminated)
    char bssid[18];             ///< Connected BSSID in format "xx:xx:xx:xx:xx:xx"
    uint8_t channel;            ///< WiFi channel
    int8_t rssi;                ///< Signal strength
    uint8_t pci_en;             ///< PCI enable status
    uint16_t reconn_interval;   ///< Reconnection interval
    uint16_t listen_interval;   ///< Listen interval
    uint8_t scan_mode;          ///< Scan mode
    uint8_t pmf;                ///< Protected Management Frame status
} bnwps_connection_info_t;

/**
 * @brief Initialize WPS subsystem
 * 
 * Sets up the WPS configuration and WiFi event handlers.
 * Must be called before using any other WPS functions.
 * 
 * @return true on success, false on failure
 */
bool bnwps_init(void);

/**
 * @brief Deinitialize WPS subsystem
 * 
 * Cleans up WPS configuration and event handlers.
 */
void bnwps_deinit(void);

/**
 * @brief Start WPS operation with timeout
 * 
 * Starts WPS (WiFi Protected Setup) operation for the specified duration.
 * The ESP32 will enter WPS mode and wait for a router to initiate WPS connection.
 * 
 * @param timeout_seconds Timeout in seconds (1-300). Use 0 to cancel ongoing WPS.
 * @return true if WPS started successfully, false on error
 */
bool bnwps_start(uint16_t timeout_seconds);

/**
 * @brief Cancel ongoing WPS operation
 * 
 * Stops any active WPS operation and returns to idle state.
 * 
 * @return true if cancelled successfully, false on error
 */
bool bnwps_cancel(void);

/**
 * @brief Get current WPS status
 * 
 * Returns the current state of WPS operation.
 * 
 * @return Current WPS status
 */
bnwps_status_t bnwps_get_status(void);

/**
 * @brief Get connection information from successful WPS
 * 
 * Retrieves connection details after a successful WPS operation.
 * Only valid when status is BNWPS_STATUS_SUCCESS.
 * 
 * @param info Pointer to structure to fill with connection information
 * @return true if information retrieved successfully, false otherwise
 */
bool bnwps_get_connection_info(bnwps_connection_info_t *info);

/**
 * @brief Check if WPS operation is currently active
 * 
 * @return true if WPS is active (waiting for connection), false otherwise
 */
bool bnwps_is_active(void);

/**
 * @brief Get remaining timeout seconds
 * 
 * Returns the number of seconds remaining in the current WPS operation.
 * 
 * @return Remaining seconds, or 0 if WPS is not active
 */
uint16_t bnwps_get_remaining_time(void);

#ifdef __cplusplus
}
#endif

#endif // BNWPS_H
