/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_at.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "bnwps_sm.h"

static const char *TAG = "BNWPS_SM";


/* Global WPS context */
static bnwps_ctx_t s_bnwps_ctx = {0};
static bool s_initialized = false;
static TaskHandle_t s_bnwps_task_handle = NULL;

/* Forward declarations */
static void bnwps_task(void *arg);
static void bnwps_timeout_callback(TimerHandle_t timer);
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void bnwps_emit_success_response(void);
static void bnwps_emit_error_response(bnwps_error_code_t error_code);

/**
 * @brief Initialize the WPS state machine
 */
esp_err_t bnwps_sm_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    /* Initialize context */
    memset(&s_bnwps_ctx, 0, sizeof(s_bnwps_ctx));
    s_bnwps_ctx.state = BNWPS_STATE_IDLE;
    s_bnwps_ctx.last_error = BNWPS_ERR_GENERAL_FAILURE;

    /* Create mutex */
    s_bnwps_ctx.mutex = xSemaphoreCreateMutex();
    if (!s_bnwps_ctx.mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Create event group */
    s_bnwps_ctx.event_group = xEventGroupCreate();
    if (!s_bnwps_ctx.event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        vSemaphoreDelete(s_bnwps_ctx.mutex);
        return ESP_ERR_NO_MEM;
    }

    /* Create timer */
    s_bnwps_ctx.timeout_timer = xTimerCreate(
        "bnwps_timer",
        pdMS_TO_TICKS(1000), /* 1 second (will be adjusted) */
        pdFALSE,             /* One shot */
        NULL,                /* Timer ID */
        bnwps_timeout_callback
    );
    if (!s_bnwps_ctx.timeout_timer) {
        ESP_LOGE(TAG, "Failed to create timer");
        vEventGroupDelete(s_bnwps_ctx.event_group);
        vSemaphoreDelete(s_bnwps_ctx.mutex);
        return ESP_ERR_NO_MEM;
    }

    /* Create task */
    BaseType_t ret = xTaskCreate(
        bnwps_task,
        "bnwps_task",
        4096,   /* Stack size */
        NULL,   /* Task parameter */
        5,      /* Priority */
        &s_bnwps_task_handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        xTimerDelete(s_bnwps_ctx.timeout_timer, portMAX_DELAY);
        vEventGroupDelete(s_bnwps_ctx.event_group);
        vSemaphoreDelete(s_bnwps_ctx.mutex);
        return ESP_ERR_NO_MEM;
    }

    /* Register event handlers */
    esp_err_t err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(err));
        /* Continue anyway - this is not fatal */
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(err));
        /* Continue anyway - this is not fatal */
    }

    s_initialized = true;
    ESP_LOGI(TAG, "WPS state machine initialized");
    return ESP_OK;
}

/**
 * @brief Deinitialize the WPS state machine
 */
esp_err_t bnwps_sm_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Cancel any active session first */
    bnwps_sm_cancel();

    /* Unregister event handlers */
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler);

    /* Delete task */
    if (s_bnwps_task_handle) {
        vTaskDelete(s_bnwps_task_handle);
        s_bnwps_task_handle = NULL;
    }

    /* Delete timer */
    if (s_bnwps_ctx.timeout_timer) {
        xTimerDelete(s_bnwps_ctx.timeout_timer, portMAX_DELAY);
        s_bnwps_ctx.timeout_timer = NULL;
    }

    /* Delete event group */
    if (s_bnwps_ctx.event_group) {
        vEventGroupDelete(s_bnwps_ctx.event_group);
        s_bnwps_ctx.event_group = NULL;
    }

    /* Delete mutex */
    if (s_bnwps_ctx.mutex) {
        vSemaphoreDelete(s_bnwps_ctx.mutex);
        s_bnwps_ctx.mutex = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "WPS state machine deinitialized");
    return ESP_OK;
}

/**
 * @brief Start WPS session with timeout
 */
esp_err_t bnwps_sm_start(uint32_t duration_sec)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;

    /* Check if already active */
    if (s_bnwps_ctx.state != BNWPS_STATE_IDLE) {
        ESP_LOGW(TAG, "WPS already active (state: %d)", s_bnwps_ctx.state);
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    /* Validate duration */
    if (duration_sec < 1 || duration_sec > CONFIG_BNWPS_MAX_DURATION) {
        ESP_LOGW(TAG, "Invalid duration: %u", (unsigned)duration_sec);
        ret = ESP_ERR_INVALID_ARG;
        goto exit;
    }

#ifdef CONFIG_BNWPS_ALLOW_RECONNECT
    /* Check if already connected and CONFIG_BNWPS_ALLOW_RECONNECT is enabled */
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK && (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "Already connected, will disconnect for WPS");
            esp_wifi_disconnect();
            vTaskDelay(pdMS_TO_TICKS(100)); /* Brief delay to allow disconnect */
        }
    }
#else
    /* Check if already connected and reject if CONFIG_BNWPS_ALLOW_RECONNECT is disabled */
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK && (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGW(TAG, "Already connected to AP, WPS rejected (ALLOW_RECONNECT disabled)");
            ret = ESP_ERR_INVALID_STATE;
            goto exit;
        }
    }
#endif

    /* Clear event group */
    xEventGroupClearBits(s_bnwps_ctx.event_group, 0xFFFFFF);

    /* Configure and start WPS */
    esp_wps_config_t wps_config = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);
    esp_err_t wps_ret = esp_wifi_wps_enable(&wps_config);
    if (wps_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable WPS: %s", esp_err_to_name(wps_ret));
        ret = wps_ret;
        goto exit;
    }

    wps_ret = esp_wifi_wps_start(0);
    if (wps_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WPS: %s", esp_err_to_name(wps_ret));
        esp_wifi_wps_disable();
        ret = wps_ret;
        goto exit;
    }

    /* Set state and duration */
    s_bnwps_ctx.state = BNWPS_STATE_ACTIVE;
    s_bnwps_ctx.duration_sec = duration_sec;

    /* Start timeout timer */
    if (xTimerChangePeriod(s_bnwps_ctx.timeout_timer, pdMS_TO_TICKS(duration_sec * 1000), 100) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timer");
        esp_wifi_wps_disable();
        s_bnwps_ctx.state = BNWPS_STATE_IDLE;
        ret = ESP_FAIL;
        goto exit;
    }

    ESP_LOGI(TAG, "WPS started for %u seconds", (unsigned)duration_sec);

exit:
    xSemaphoreGive(s_bnwps_ctx.mutex);
    return ret;
}

/**
 * @brief Cancel active WPS session
 */
esp_err_t bnwps_sm_cancel(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;

    if (s_bnwps_ctx.state == BNWPS_STATE_ACTIVE) {
        /* Stop timer */
        xTimerStop(s_bnwps_ctx.timeout_timer, 100);

        /* Disable WPS */
        esp_wifi_wps_disable();

        /* Set state */
        s_bnwps_ctx.state = BNWPS_STATE_CANCELED;
        s_bnwps_ctx.last_error = BNWPS_ERR_CANCELED;

        ESP_LOGI(TAG, "WPS session canceled");

        /* Set event to notify task */
        xEventGroupSetBits(s_bnwps_ctx.event_group, BNWPS_EVENT_WPS_FAILED);
    } else {
        ESP_LOGD(TAG, "No active WPS session to cancel");
    }

    xSemaphoreGive(s_bnwps_ctx.mutex);
    return ret;
}

/**
 * @brief Query current WPS state
 */
bool bnwps_sm_is_active(void)
{
    if (!s_initialized) {
        return false;
    }

    bool active = false;
    if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        active = (s_bnwps_ctx.state == BNWPS_STATE_ACTIVE);
        xSemaphoreGive(s_bnwps_ctx.mutex);
    }
    return active;
}

/**
 * @brief Get current WPS state
 */
bnwps_state_t bnwps_sm_get_state(void)
{
    if (!s_initialized) {
        return BNWPS_STATE_IDLE;
    }

    bnwps_state_t state = BNWPS_STATE_IDLE;
    if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = s_bnwps_ctx.state;
        xSemaphoreGive(s_bnwps_ctx.mutex);
    }
    return state;
}

/**
 * @brief Get connection information for successful connections
 */
esp_err_t bnwps_sm_get_conn_info(bnwps_ctx_t *ctx)
{
    if (!s_initialized || !ctx) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (s_bnwps_ctx.state == BNWPS_STATE_CONNECTED) {
        memcpy(ctx, &s_bnwps_ctx, sizeof(bnwps_ctx_t));
        xSemaphoreGive(s_bnwps_ctx.mutex);
        return ESP_OK;
    }

    xSemaphoreGive(s_bnwps_ctx.mutex);
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Get last error code for failed connections
 */
bnwps_error_code_t bnwps_sm_get_last_error(void)
{
    if (!s_initialized) {
        return BNWPS_ERR_NOT_INITIALIZED;
    }

    bnwps_error_code_t error = BNWPS_ERR_GENERAL_FAILURE;
    if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        error = s_bnwps_ctx.last_error;
        xSemaphoreGive(s_bnwps_ctx.mutex);
    }
    return error;
}

/**
 * @brief Timer timeout callback
 */
static void bnwps_timeout_callback(TimerHandle_t timer)
{
    ESP_LOGW(TAG, "WPS timeout");
    
    /* Set timeout event */
    if (s_bnwps_ctx.event_group) {
        xEventGroupSetBits(s_bnwps_ctx.event_group, BNWPS_EVENT_WPS_TIMEOUT);
    }
}

/**
 * @brief WiFi event handler
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (!s_initialized || !s_bnwps_ctx.event_group) {
        return;
    }

    switch (event_id) {
        case WIFI_EVENT_STA_WPS_ER_SUCCESS:
            ESP_LOGI(TAG, "WPS success event");
            
            /* Get connection info */
            if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                wifi_event_sta_wps_er_success_t *evt = (wifi_event_sta_wps_er_success_t *)event_data;
                
                if (evt && evt->ap_cred_cnt > 0) {
                    /* Copy SSID - ensure null termination */
                    size_t ssid_len = strnlen((char *)evt->ap_cred[0].ssid, sizeof(evt->ap_cred[0].ssid));
                    memcpy(s_bnwps_ctx.ssid, evt->ap_cred[0].ssid, ssid_len);
                    s_bnwps_ctx.ssid[ssid_len] = '\0';
                    
                    ESP_LOGI(TAG, "WPS credentials received for SSID: %s", s_bnwps_ctx.ssid);
                }
                
                xSemaphoreGive(s_bnwps_ctx.mutex);
            }
            
            xEventGroupSetBits(s_bnwps_ctx.event_group, BNWPS_EVENT_WPS_SUCCESS);
            break;

        case WIFI_EVENT_STA_WPS_ER_FAILED:
            ESP_LOGW(TAG, "WPS failed event");
            xEventGroupSetBits(s_bnwps_ctx.event_group, BNWPS_EVENT_WPS_FAILED);
            break;

        case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
            ESP_LOGW(TAG, "WPS timeout event");
            xEventGroupSetBits(s_bnwps_ctx.event_group, BNWPS_EVENT_WPS_TIMEOUT);
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected event");
            
            /* Get detailed connection info */
            if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                wifi_event_sta_connected_t *evt = (wifi_event_sta_connected_t *)event_data;
                if (evt) {
                    /* Update connection details */
                    s_bnwps_ctx.channel = evt->channel;
                    snprintf(s_bnwps_ctx.bssid, sizeof(s_bnwps_ctx.bssid), 
                             "%02x:%02x:%02x:%02x:%02x:%02x",
                             evt->bssid[0], evt->bssid[1], evt->bssid[2],
                             evt->bssid[3], evt->bssid[4], evt->bssid[5]);
                    
                    /* Set default values for missing fields */
                    s_bnwps_ctx.rssi = -50;  /* Will be updated when we get AP info */
                    s_bnwps_ctx.pci_en = 1;
                    s_bnwps_ctx.reconn_interval = 0;
                    s_bnwps_ctx.listen_interval = 0;
                    s_bnwps_ctx.scan_mode = 0;
                    s_bnwps_ctx.pmf = 1;
                }
                xSemaphoreGive(s_bnwps_ctx.mutex);
            }
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi disconnected event");
            if (bnwps_sm_is_active()) {
                xEventGroupSetBits(s_bnwps_ctx.event_group, BNWPS_EVENT_WPS_DISCONN);
            }
            break;

        default:
            break;
    }
}

/**
 * @brief IP event handler
 */
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (!s_initialized || !s_bnwps_ctx.event_group) {
        return;
    }

    if (event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Got IP event");
        
        /* Update RSSI from AP info if available */
        if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                s_bnwps_ctx.rssi = ap_info.rssi;
                ESP_LOGD(TAG, "Updated RSSI: %d", s_bnwps_ctx.rssi);
            }
            xSemaphoreGive(s_bnwps_ctx.mutex);
        }
        
        xEventGroupSetBits(s_bnwps_ctx.event_group, BNWPS_EVENT_GOT_IP);
    }
}

/**
 * @brief Emit success response in CWJAP format
 */
static void bnwps_emit_success_response(void)
{
    char response[256];
    
    if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int len = snprintf(response, sizeof(response),
            "+CWJAP:\"%s\",\"%s\",%d,%d,%u,%u,%u,%u,%u\r\n",
            s_bnwps_ctx.ssid,
            s_bnwps_ctx.bssid,
            s_bnwps_ctx.channel,
            s_bnwps_ctx.rssi,
            s_bnwps_ctx.pci_en,
            s_bnwps_ctx.reconn_interval,
            s_bnwps_ctx.listen_interval,
            s_bnwps_ctx.scan_mode,
            s_bnwps_ctx.pmf);
        
        xSemaphoreGive(s_bnwps_ctx.mutex);
        
        if (len > 0 && len < sizeof(response)) {
            esp_at_port_write_data((uint8_t *)response, len);
        }
    }
}

/**
 * @brief Emit error response in CWJAP format
 */
static void bnwps_emit_error_response(bnwps_error_code_t error_code)
{
    char response[32];
    int len = snprintf(response, sizeof(response), "+CWJAP:%d\r\n", error_code);
    
    if (len > 0 && len < sizeof(response)) {
        esp_at_port_write_data((uint8_t *)response, len);
    }
}

/**
 * @brief Main WPS task
 */
static void bnwps_task(void *arg)
{
    ESP_LOGI(TAG, "WPS task started");
    
    while (1) {
        if (!s_initialized) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        /* Wait for events */
        EventBits_t bits = xEventGroupWaitBits(
            s_bnwps_ctx.event_group,
            BNWPS_EVENT_WPS_SUCCESS | BNWPS_EVENT_WPS_FAILED | 
            BNWPS_EVENT_WPS_TIMEOUT | BNWPS_EVENT_WPS_DISCONN | 
            BNWPS_EVENT_GOT_IP,
            pdTRUE,   /* Clear on exit */
            pdFALSE,  /* Wait for any bit */
            pdMS_TO_TICKS(5000)  /* 5 second timeout */
        );

        if (bits == 0) {
            /* Timeout - continue waiting */
            continue;
        }

        /* Handle events */
        if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            if (bits & BNWPS_EVENT_WPS_SUCCESS) {
                ESP_LOGI(TAG, "Processing WPS success");
                
                /* WPS succeeded, wait for connection and IP */
                xSemaphoreGive(s_bnwps_ctx.mutex);
                
                /* Wait for got IP event (up to 30 seconds) */
                EventBits_t ip_bits = xEventGroupWaitBits(
                    s_bnwps_ctx.event_group,
                    BNWPS_EVENT_GOT_IP | BNWPS_EVENT_WPS_DISCONN,
                    pdTRUE,   /* Clear on exit */
                    pdFALSE,  /* Wait for any bit */
                    pdMS_TO_TICKS(30000)  /* 30 second timeout */
                );
                
                if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    if (ip_bits & BNWPS_EVENT_GOT_IP) {
                        /* Success! */
                        ESP_LOGI(TAG, "WPS connection successful");
                        s_bnwps_ctx.state = BNWPS_STATE_CONNECTED;
                        
                        /* Stop timer and disable WPS */
                        xTimerStop(s_bnwps_ctx.timeout_timer, 100);
                        esp_wifi_wps_disable();
                        
                        xSemaphoreGive(s_bnwps_ctx.mutex);
                        
                        /* Emit success response */
                        bnwps_emit_success_response();
                        esp_at_port_write_data((uint8_t *)"OK\r\n", 4);
                        
                        if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            s_bnwps_ctx.state = BNWPS_STATE_IDLE;
                            xSemaphoreGive(s_bnwps_ctx.mutex);
                        }
                    } else {
                        /* Failed to get IP or disconnected */
                        ESP_LOGW(TAG, "Failed to get IP after WPS success");
                        s_bnwps_ctx.state = BNWPS_STATE_FAILED;
                        s_bnwps_ctx.last_error = BNWPS_ERR_GENERAL_FAILURE;
                        
                        /* Stop timer and disable WPS */
                        xTimerStop(s_bnwps_ctx.timeout_timer, 100);
                        esp_wifi_wps_disable();
                        
                        xSemaphoreGive(s_bnwps_ctx.mutex);
                        
                        /* Emit error response */
                        bnwps_emit_error_response(BNWPS_ERR_GENERAL_FAILURE);
                        esp_at_port_write_data((uint8_t *)"ERROR\r\n", 7);
                        
                        if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            s_bnwps_ctx.state = BNWPS_STATE_IDLE;
                            xSemaphoreGive(s_bnwps_ctx.mutex);
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to take mutex in success handler");
                }
                
            } else if (bits & (BNWPS_EVENT_WPS_FAILED | BNWPS_EVENT_WPS_TIMEOUT | BNWPS_EVENT_WPS_DISCONN)) {
                /* WPS failed */
                bnwps_error_code_t error_code = BNWPS_ERR_GENERAL_FAILURE;
                
                if (bits & BNWPS_EVENT_WPS_TIMEOUT) {
                    ESP_LOGW(TAG, "WPS timeout");
                    error_code = BNWPS_ERR_TIMEOUT;
                } else if (bits & BNWPS_EVENT_WPS_FAILED) {
                    if (s_bnwps_ctx.state == BNWPS_STATE_CANCELED) {
                        ESP_LOGI(TAG, "WPS canceled");
                        error_code = BNWPS_ERR_CANCELED;
                    } else {
                        ESP_LOGW(TAG, "WPS failed");
                        error_code = BNWPS_ERR_WPS_FAILED;
                    }
                } else if (bits & BNWPS_EVENT_WPS_DISCONN) {
                    ESP_LOGW(TAG, "WPS disconnected");
                    error_code = BNWPS_ERR_GENERAL_FAILURE;
                }
                
                s_bnwps_ctx.state = BNWPS_STATE_FAILED;
                s_bnwps_ctx.last_error = error_code;
                
                /* Stop timer and disable WPS */
                xTimerStop(s_bnwps_ctx.timeout_timer, 100);
                esp_wifi_wps_disable();
                
                xSemaphoreGive(s_bnwps_ctx.mutex);
                
                /* Emit appropriate response based on error type */
                if (error_code == BNWPS_ERR_CANCELED) {
                    /* For cancel, we already sent +BNWPS:0 in the cancel function */
                    /* Don't send additional error response */
                } else {
                    /* Emit error response */
                    bnwps_emit_error_response(error_code);
                    esp_at_port_write_data((uint8_t *)"ERROR\r\n", 7);
                }
                
                if (xSemaphoreTake(s_bnwps_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    s_bnwps_ctx.state = BNWPS_STATE_IDLE;
                    xSemaphoreGive(s_bnwps_ctx.mutex);
                }
            } else {
                xSemaphoreGive(s_bnwps_ctx.mutex);
            }
        }
    }
}
