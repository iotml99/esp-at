/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_at.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "at_cmd_bnwps.h"
#include "bnwps_sm.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

static const char *TAG = "BNWPS";



/**
 * @brief AT+BNWPS=? (test command)
 * Shows usage information
 */
uint8_t at_bnwps_cmd_test(uint8_t *cmd_name)
{
    const char *usage = 
        "AT+BNWPS commands:\r\n"
        "  AT+BNWPS=<t>    Start WPS PBC for <t> seconds (1-" TOSTRING(CONFIG_BNWPS_MAX_DURATION) ")\r\n"
        "  AT+BNWPS?       Query WPS state (1=active, 0=idle)\r\n"
        "  AT+BNWPS=0      Cancel active WPS session\r\n"
        "\r\n"
        "On success: +CWJAP:\"<ssid>\",\"<bssid>\",<ch>,<rssi>,<pci>,<reconn>,<listen>,<scan>,<pmf>\r\n"
        "On failure: +CWJAP:<error_code>\r\n"
        "\r\n"
        "Error codes:\r\n"
        "  1=General failure, 2=Timeout, 3=WPS failed, 4=Invalid args\r\n"
        "  5=Not initialized, 6=Busy, 7=Canceled, 8=Auth failed, 9=Not supported\r\n";
    
    esp_at_port_write_data((uint8_t *)usage, strlen(usage));
    return ESP_AT_RESULT_CODE_OK;
}

/**
 * @brief AT+BNWPS? (query command) 
 * Returns current WPS state
 */
uint8_t at_bnwps_cmd_query(uint8_t *cmd_name)
{
    char response[32];
    int active = bnwps_sm_is_active() ? 1 : 0;
    
    snprintf(response, sizeof(response), "+BNWPS:%d\r\n", active);
    esp_at_port_write_data((uint8_t *)response, strlen(response));
    
    return ESP_AT_RESULT_CODE_OK;
}

/**
 * @brief AT+BNWPS=<t> (setup command)
 * Start WPS session or cancel (t=0)
 */
uint8_t at_bnwps_cmd_setup(uint8_t para_num)
{
    if (para_num != 1) {
        ESP_LOGW(TAG, "Invalid parameter count: %d", para_num);
        char error[32];
        snprintf(error, sizeof(error), "+CWJAP:%d\r\n", BNWPS_ERR_INVALID_ARGS);
        esp_at_port_write_data((uint8_t *)error, strlen(error));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    int32_t duration;
    if (esp_at_get_para_as_digit(0, &duration) != ESP_AT_PARA_PARSE_RESULT_OK) {
        ESP_LOGW(TAG, "Failed to parse duration parameter");
        char error[32];
        snprintf(error, sizeof(error), "+CWJAP:%d\r\n", BNWPS_ERR_INVALID_ARGS);
        esp_at_port_write_data((uint8_t *)error, strlen(error));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Handle cancel case */
    if (duration == 0) {
        esp_err_t ret = bnwps_sm_cancel();
        if (ret == ESP_OK) {
            esp_at_port_write_data((uint8_t *)"+BNWPS:0\r\n", 10);
            return ESP_AT_RESULT_CODE_OK;
        } else {
            ESP_LOGW(TAG, "Failed to cancel WPS: %s", esp_err_to_name(ret));
            char error[32];
            snprintf(error, sizeof(error), "+CWJAP:%d\r\n", BNWPS_ERR_GENERAL_FAILURE);
            esp_at_port_write_data((uint8_t *)error, strlen(error));
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }

    /* Validate duration range */
    if (duration < 1 || duration > CONFIG_BNWPS_MAX_DURATION) {
        ESP_LOGW(TAG, "Duration out of range: %d (valid: 1-%d)", (int)duration, CONFIG_BNWPS_MAX_DURATION);
        char error[32];
        snprintf(error, sizeof(error), "+CWJAP:%d\r\n", BNWPS_ERR_INVALID_ARGS);
        esp_at_port_write_data((uint8_t *)error, strlen(error));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Check if already active */
    if (bnwps_sm_is_active()) {
        ESP_LOGW(TAG, "WPS session already active");
        char error[32];
        snprintf(error, sizeof(error), "+CWJAP:%d\r\n", BNWPS_ERR_BUSY);
        esp_at_port_write_data((uint8_t *)error, strlen(error));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Start WPS session */
    esp_err_t ret = bnwps_sm_start((uint32_t)duration);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WPS: %s", esp_err_to_name(ret));
        bnwps_error_code_t err_code = BNWPS_ERR_GENERAL_FAILURE;
        
        /* Map specific errors */
        if (ret == ESP_ERR_INVALID_STATE) {
            err_code = BNWPS_ERR_BUSY;
        } else if (ret == ESP_ERR_NOT_SUPPORTED) {
            err_code = BNWPS_ERR_NOT_SUPPORTED;
        }
        
        char error[32];
        snprintf(error, sizeof(error), "+CWJAP:%d\r\n", err_code);
        esp_at_port_write_data((uint8_t *)error, strlen(error));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Immediate acknowledgment that WPS is started */
    esp_at_port_write_data((uint8_t *)"+BNWPS:1\r\n", 10);
    
    ESP_LOGI(TAG, "WPS session started for %d seconds", (int)duration);
    return ESP_AT_RESULT_CODE_OK;
}

/**
 * @brief Initialize BNWPS AT commands
 */
esp_err_t bnwps_init(void)
{
    esp_err_t ret = bnwps_sm_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WPS state machine: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "BNWPS subsystem initialized");
    return ESP_OK;
}

/**
 * @brief Deinitialize BNWPS AT commands
 */
esp_err_t bnwps_deinit(void)
{
    esp_err_t ret = bnwps_sm_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize WPS state machine: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "BNWPS subsystem deinitialized");
    return ESP_OK;
}
