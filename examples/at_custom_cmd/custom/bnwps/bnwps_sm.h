/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "at_cmd_bnwps.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WPS state machine states
 */
typedef enum {
    BNWPS_STATE_IDLE = 0,               ///< No WPS session active
    BNWPS_STATE_ACTIVE,                 ///< WPS session running (timer active)
    BNWPS_STATE_CONNECTED,              ///< Successfully connected via WPS
    BNWPS_STATE_FAILED,                 ///< WPS session failed
    BNWPS_STATE_CANCELED,               ///< WPS session canceled by user
} bnwps_state_t;

/**
 * @brief WPS context structure
 */
typedef struct {
    bnwps_state_t state;                ///< Current state
    SemaphoreHandle_t mutex;            ///< Mutex for thread safety
    TimerHandle_t timeout_timer;        ///< One-shot timer for WPS timeout
    EventGroupHandle_t event_group;     ///< Event group for Wi-Fi events
    uint32_t duration_sec;              ///< WPS duration in seconds
    
    /* Connection information for success case */
    char ssid[33];                      ///< Connected SSID
    char bssid[18];                     ///< Connected BSSID (MAC address)
    int8_t channel;                     ///< Connected channel
    int8_t rssi;                        ///< Signal strength
    uint8_t pci_en;                     ///< Power constraint indicator
    uint16_t reconn_interval;           ///< Reconnect interval
    uint16_t listen_interval;           ///< Listen interval
    uint8_t scan_mode;                  ///< Scan mode
    uint8_t pmf;                        ///< Protected Management Frames
    
    /* Error information for failure case */
    bnwps_error_code_t last_error;      ///< Last error code
} bnwps_ctx_t;

/* Event bits for the event group */
#define BNWPS_EVENT_WPS_SUCCESS     BIT0
#define BNWPS_EVENT_WPS_FAILED      BIT1
#define BNWPS_EVENT_WPS_TIMEOUT     BIT2
#define BNWPS_EVENT_WPS_DISCONN     BIT3
#define BNWPS_EVENT_GOT_IP          BIT4

/**
 * @brief Initialize the WPS state machine
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bnwps_sm_init(void);

/**
 * @brief Deinitialize the WPS state machine
 * 
 * @return esp_err_t ESP_OK on success  
 */
esp_err_t bnwps_sm_deinit(void);

/**
 * @brief Start WPS session with timeout
 * 
 * @param duration_sec Duration in seconds (1-120)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bnwps_sm_start(uint32_t duration_sec);

/**
 * @brief Cancel active WPS session
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bnwps_sm_cancel(void);

/**
 * @brief Query current WPS state
 * 
 * @return true if WPS session is active, false otherwise
 */
bool bnwps_sm_is_active(void);

/**
 * @brief Get current WPS state
 * 
 * @return bnwps_state_t Current state
 */
bnwps_state_t bnwps_sm_get_state(void);

/**
 * @brief Get connection information for successful connections
 * 
 * @param ctx Pointer to receive connection info
 * @return esp_err_t ESP_OK if valid info available
 */
esp_err_t bnwps_sm_get_conn_info(bnwps_ctx_t *ctx);

/**
 * @brief Get last error code for failed connections
 * 
 * @return bnwps_error_code_t Last error code
 */
bnwps_error_code_t bnwps_sm_get_last_error(void);

#ifdef __cplusplus
}
#endif
