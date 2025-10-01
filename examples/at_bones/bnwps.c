/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bnwps.h"
#include "bn_constants.h"
#include "esp_wifi.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_at.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "BNWPS";

// WPS configuration
static esp_wps_config_t s_wps_config = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);
static wifi_config_t s_wps_ap_creds[MAX_WPS_AP_CRED];
static int s_ap_creds_num = 0;

// WPS state management
static bnwps_status_t s_wps_status = BNWPS_STATUS_IDLE;
static bnwps_connection_info_t s_connection_info;
static bool s_wps_initialized = false;
static uint16_t s_timeout_seconds = 0;
static uint16_t s_remaining_seconds = 0;
static TimerHandle_t s_wps_timer = NULL;
static TimerHandle_t s_countdown_timer = NULL;

// Forward declarations
static void bnwps_wifi_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data);
static void bnwps_ip_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data);
static void bnwps_timeout_callback(TimerHandle_t xTimer);
static void bnwps_countdown_callback(TimerHandle_t xTimer);
static bool bnwps_extract_connection_info(void);
static void bnwps_send_status_update(void);

bool bnwps_init(void)
{
    if (s_wps_initialized) {
        ESP_LOGW(TAG, "WPS already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing WPS subsystem");

    // Create timers
    s_wps_timer = xTimerCreate("wps_timeout", pdMS_TO_TICKS(1000), pdFALSE, NULL, bnwps_timeout_callback);
    s_countdown_timer = xTimerCreate("wps_countdown", pdMS_TO_TICKS(1000), pdTRUE, NULL, bnwps_countdown_callback);
    
    if (!s_wps_timer || !s_countdown_timer) {
        ESP_LOGE(TAG, "Failed to create WPS timers");
        return false;
    }

    // Register event handlers
    esp_err_t err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &bnwps_wifi_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &bnwps_ip_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(err));
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &bnwps_wifi_event_handler);
        return false;
    }

    // Initialize WPS state
    s_wps_status = BNWPS_STATUS_IDLE;
    s_ap_creds_num = 0;
    s_timeout_seconds = 0;
    s_remaining_seconds = 0;
    memset(&s_connection_info, 0, sizeof(s_connection_info));
    memset(s_wps_ap_creds, 0, sizeof(s_wps_ap_creds));

    s_wps_initialized = true;
    ESP_LOGI(TAG, "WPS subsystem initialized successfully");
    return true;
}

void bnwps_deinit(void)
{
    if (!s_wps_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing WPS subsystem");

    // Cancel any ongoing WPS operation
    bnwps_cancel();

    // Delete timers
    if (s_wps_timer) {
        xTimerDelete(s_wps_timer, portMAX_DELAY);
        s_wps_timer = NULL;
    }
    if (s_countdown_timer) {
        xTimerDelete(s_countdown_timer, portMAX_DELAY);
        s_countdown_timer = NULL;
    }

    // Unregister event handlers
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &bnwps_wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &bnwps_ip_event_handler);

    s_wps_initialized = false;
    ESP_LOGI(TAG, "WPS subsystem deinitialized");
}

bool bnwps_start(uint16_t timeout_seconds)
{
    if (!s_wps_initialized) {
        ESP_LOGE(TAG, "WPS not initialized");
        return false;
    }

    // Handle cancel request (timeout_seconds = 0)
    if (timeout_seconds == 0) {
        return bnwps_cancel();
    }

    // Validate timeout
    if (timeout_seconds > BNWPS_MAX_TIMEOUT_SECONDS) {
        ESP_LOGE(TAG, "Timeout too large: %u seconds (max: %u)", timeout_seconds, BNWPS_MAX_TIMEOUT_SECONDS);
        return false;
    }

    // Cancel any existing WPS operation
    if (s_wps_status == BNWPS_STATUS_ACTIVE) {
        ESP_LOGW(TAG, "Cancelling existing WPS operation");
        bnwps_cancel();
    }

    ESP_LOGI(TAG, "Starting WPS operation with %u second timeout", timeout_seconds);

    // Initialize WPS state
    s_wps_status = BNWPS_STATUS_ACTIVE;
    s_timeout_seconds = timeout_seconds;
    s_remaining_seconds = timeout_seconds;
    s_ap_creds_num = 0;
    memset(&s_connection_info, 0, sizeof(s_connection_info));
    memset(s_wps_ap_creds, 0, sizeof(s_wps_ap_creds));

    // Enable and start WPS
    esp_err_t err = esp_wifi_wps_enable(&s_wps_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable WPS: %s", esp_err_to_name(err));
        s_wps_status = BNWPS_STATUS_FAILED;
        return false;
    }

    err = esp_wifi_wps_start(0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WPS: %s", esp_err_to_name(err));
        esp_wifi_wps_disable();
        s_wps_status = BNWPS_STATUS_FAILED;
        return false;
    }

    // Start timeout timer
    xTimerChangePeriod(s_wps_timer, pdMS_TO_TICKS(timeout_seconds * 1000), portMAX_DELAY);
    xTimerStart(s_wps_timer, portMAX_DELAY);

    // Start countdown timer for status updates
    xTimerStart(s_countdown_timer, portMAX_DELAY);

    // Send initial status update
    bnwps_send_status_update();

    ESP_LOGI(TAG, "WPS operation started successfully");
    return true;
}

bool bnwps_cancel(void)
{
    if (!s_wps_initialized) {
        ESP_LOGE(TAG, "WPS not initialized");
        return false;
    }

    if (s_wps_status != BNWPS_STATUS_ACTIVE) {
        ESP_LOGW(TAG, "No active WPS operation to cancel");
        s_wps_status = BNWPS_STATUS_IDLE;
        return true;
    }

    ESP_LOGI(TAG, "Cancelling WPS operation");

    // Stop timers
    xTimerStop(s_wps_timer, portMAX_DELAY);
    xTimerStop(s_countdown_timer, portMAX_DELAY);

    // Disable WPS
    esp_err_t err = esp_wifi_wps_disable();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to disable WPS: %s", esp_err_to_name(err));
    }

    // Update status
    s_wps_status = BNWPS_STATUS_IDLE;
    s_timeout_seconds = 0;
    s_remaining_seconds = 0;

    // Send status update
    bnwps_send_status_update();

    ESP_LOGI(TAG, "WPS operation cancelled");
    return true;
}

bnwps_status_t bnwps_get_status(void)
{
    return s_wps_status;
}

bool bnwps_get_connection_info(bnwps_connection_info_t *info)
{
    if (!info) {
        ESP_LOGE(TAG, "Invalid parameter");
        return false;
    }

    if (s_wps_status != BNWPS_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "No successful connection information available");
        return false;
    }

    memcpy(info, &s_connection_info, sizeof(bnwps_connection_info_t));
    return true;
}

bool bnwps_is_active(void)
{
    return (s_wps_status == BNWPS_STATUS_ACTIVE);
}

uint16_t bnwps_get_remaining_time(void)
{
    return s_remaining_seconds;
}

// Event handlers
static void bnwps_wifi_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data)
{
    switch (event_id) {
        case WIFI_EVENT_STA_WPS_ER_SUCCESS:
            ESP_LOGI(TAG, "WPS connection successful");
            {
                wifi_event_sta_wps_er_success_t *evt = (wifi_event_sta_wps_er_success_t *)event_data;
                
                if (evt) {
                    s_ap_creds_num = evt->ap_cred_cnt;
                    for (int i = 0; i < s_ap_creds_num; i++) {
                        memcpy(s_wps_ap_creds[i].sta.ssid, evt->ap_cred[i].ssid,
                               sizeof(evt->ap_cred[i].ssid));
                        memcpy(s_wps_ap_creds[i].sta.password, evt->ap_cred[i].passphrase,
                               sizeof(evt->ap_cred[i].passphrase));
                    }
                    
                    // Configure WiFi with first credential
                    if (s_ap_creds_num > 0) {
                        ESP_LOGI(TAG, "Connecting to SSID: %s", s_wps_ap_creds[0].sta.ssid);
                        esp_wifi_set_config(WIFI_IF_STA, &s_wps_ap_creds[0]);
                    }
                }
                
                // Disable WPS and connect
                esp_wifi_wps_disable();
                esp_wifi_connect();
            }
            break;

        case WIFI_EVENT_STA_WPS_ER_FAILED:
            ESP_LOGI(TAG, "WPS connection failed");
            s_wps_status = BNWPS_STATUS_FAILED;
            xTimerStop(s_wps_timer, portMAX_DELAY);
            xTimerStop(s_countdown_timer, portMAX_DELAY);
            esp_wifi_wps_disable();
            
            // Send error response
            {
                char error_msg[BN_BUFFER_MEDIUM];
                snprintf(error_msg, sizeof(error_msg), "+CWJAP:1\r\nERROR\r\n");
                esp_at_port_write_data((uint8_t *)error_msg, strlen(error_msg));
            }
            break;

        case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
            ESP_LOGI(TAG, "WPS operation timed out");
            s_wps_status = BNWPS_STATUS_TIMEOUT;
            xTimerStop(s_wps_timer, portMAX_DELAY);
            xTimerStop(s_countdown_timer, portMAX_DELAY);
            esp_wifi_wps_disable();
            
            // Send timeout error response
            {
                char error_msg[BN_BUFFER_MEDIUM];
                snprintf(error_msg, sizeof(error_msg), "+CWJAP:2\r\nERROR\r\n");
                esp_at_port_write_data((uint8_t *)error_msg, strlen(error_msg));
            }
            break;

        case WIFI_EVENT_STA_WPS_ER_PIN:
            ESP_LOGI(TAG, "WPS PIN event (not used in PBC mode)");
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected via WPS");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WiFi disconnected");
            if (s_wps_status == BNWPS_STATUS_SUCCESS) {
                // Connection was established but lost - this is handled by normal WiFi reconnection
                ESP_LOGW(TAG, "WiFi connection lost after successful WPS");
            }
            break;

        default:
            break;
    }
}

static void bnwps_ip_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Got IP address - WPS connection complete");
        
        // Stop timers
        xTimerStop(s_wps_timer, portMAX_DELAY);
        xTimerStop(s_countdown_timer, portMAX_DELAY);
        
        // Update status and extract connection info
        s_wps_status = BNWPS_STATUS_SUCCESS;
        bnwps_extract_connection_info();
        
        // Send success response in AT+CWJAP format
        char response[256];
        snprintf(response, sizeof(response), 
                "+CWJAP:\"%s\",\"%s\",%u,%d,%u,%u,%u,%u,%u\r\nOK\r\n",
                s_connection_info.ssid,
                s_connection_info.bssid,
                s_connection_info.channel,
                s_connection_info.rssi,
                s_connection_info.pci_en,
                s_connection_info.reconn_interval,
                s_connection_info.listen_interval,
                s_connection_info.scan_mode,
                s_connection_info.pmf);
        esp_at_port_write_data((uint8_t *)response, strlen(response));
    }
}

static void bnwps_timeout_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "WPS timeout reached");
    s_wps_status = BNWPS_STATUS_TIMEOUT;
    s_remaining_seconds = 0;
    
    // Stop countdown timer and disable WPS
    xTimerStop(s_countdown_timer, portMAX_DELAY);
    esp_wifi_wps_disable();
    
    // Send timeout error response
    char error_msg[BN_BUFFER_MEDIUM];
    snprintf(error_msg, sizeof(error_msg), "+CWJAP:2\r\nERROR\r\n");
    esp_at_port_write_data((uint8_t *)error_msg, strlen(error_msg));
}

static void bnwps_countdown_callback(TimerHandle_t xTimer)
{
    if (s_wps_status == BNWPS_STATUS_ACTIVE && s_remaining_seconds > 0) {
        s_remaining_seconds--;
        ESP_LOGD(TAG, "WPS remaining time: %u seconds", s_remaining_seconds);
    }
}

static bool bnwps_extract_connection_info(void)
{
    // Get current WiFi configuration
    wifi_config_t wifi_config;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WiFi config: %s", esp_err_to_name(err));
        return false;
    }

    // Copy SSID
    strncpy(s_connection_info.ssid, (char *)wifi_config.sta.ssid, sizeof(s_connection_info.ssid) - 1);
    s_connection_info.ssid[sizeof(s_connection_info.ssid) - 1] = '\0';

    // Get AP info
    wifi_ap_record_t ap_info;
    err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err == ESP_OK) {
        // Format BSSID
        snprintf(s_connection_info.bssid, sizeof(s_connection_info.bssid),
                "%02x:%02x:%02x:%02x:%02x:%02x",
                ap_info.bssid[0], ap_info.bssid[1], ap_info.bssid[2],
                ap_info.bssid[3], ap_info.bssid[4], ap_info.bssid[5]);
        
        s_connection_info.channel = ap_info.primary;
        s_connection_info.rssi = ap_info.rssi;
    } else {
        ESP_LOGW(TAG, "Failed to get AP info: %s", esp_err_to_name(err));
        strcpy(s_connection_info.bssid, "00:00:00:00:00:00");
        s_connection_info.channel = 0;
        s_connection_info.rssi = 0;
    }

    // Set default values for other fields (these would need to be obtained from WiFi stack)
    s_connection_info.pci_en = 0;
    s_connection_info.reconn_interval = 0;
    s_connection_info.listen_interval = 0;
    s_connection_info.scan_mode = 0;
    s_connection_info.pmf = 0;

    ESP_LOGI(TAG, "Connection info extracted: SSID=%s, BSSID=%s, CH=%u, RSSI=%d",
             s_connection_info.ssid, s_connection_info.bssid, 
             s_connection_info.channel, s_connection_info.rssi);

    return true;
}

static void bnwps_send_status_update(void)
{
    // This function can be used to send periodic status updates if needed
    // Currently not implemented as per the specification
}
