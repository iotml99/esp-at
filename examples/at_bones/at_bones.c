/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "esp_at.h"
#include "bncurl_params.h"
#include "bncurl.h"
#include "bncurl_config.h"


static bncurl_context_t *bncurl_ctx = NULL;


static uint8_t at_test_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "test command: <AT%s=?> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));

    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_query_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "query command: <AT%s?> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));

    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_setup_cmd_test(uint8_t para_num)
{
    // Use the BNCURL parameter parser to parse and print parameters
    return bncurl_parse_and_print_params(para_num, &bncurl_ctx->params);
}

static uint8_t at_exe_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "execute command: <AT%s> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));

    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_timeout_test(uint8_t *cmd_name)
{
    uint8_t buffer[128] = {0};
    snprintf((char *)buffer, 128, 
        "AT+BNCURL_TIMEOUT=<timeout>\r\n"
        "Set timeout for server reaction in seconds.\r\n"
        "Range: %d-%d seconds\r\n", 
        BNCURL_MIN_TIMEOUT, BNCURL_MAX_TIMEOUT);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_timeout_query(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    uint32_t timeout = bncurl_get_timeout(bncurl_ctx);
    if (timeout == 0) {
        timeout = BNCURL_DEFAULT_TIMEOUT;  // Return default if not set
    }
    snprintf((char *)buffer, 64, "+BNCURL_TIMEOUT:%u\r\n", timeout);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_timeout_setup(uint8_t para_num)
{
    if (para_num != 1) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    int32_t timeout = 0;
    if (esp_at_get_para_as_digit(0, &timeout) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    if (timeout < 0) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (timeout < BNCURL_MIN_TIMEOUT || timeout > BNCURL_MAX_TIMEOUT) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (!bncurl_set_timeout(bncurl_ctx, (uint32_t)timeout)) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    return ESP_AT_RESULT_CODE_OK;
}

static const esp_at_cmd_struct at_custom_cmd[] = {
    {"+BNCURL", at_test_cmd_test, at_query_cmd_test, at_setup_cmd_test, at_exe_cmd_test},
    {"+BNCURL_TIMEOUT", at_bncurl_timeout_test, at_bncurl_timeout_query, at_bncurl_timeout_setup, NULL}
};

bool esp_at_custom_cmd_register(void)
{
    bncurl_ctx = calloc(1, sizeof(bncurl_context_t));
    if(!bncurl_ctx) {
        return false;
    }
    if(!bncurl_init(bncurl_ctx)) {
        return false;
    }
    return esp_at_custom_cmd_array_regist(at_custom_cmd, sizeof(at_custom_cmd) / sizeof(esp_at_cmd_struct));
}

ESP_AT_CMD_SET_INIT_FN(esp_at_custom_cmd_register, 1);