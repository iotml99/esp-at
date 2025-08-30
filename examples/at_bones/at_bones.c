/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_at.h"
#include "bncurl_params.h"
#include "bncurl.h"
#include "bncurl_config.h"
#include "bncurl_executor.h"
#include "at_sd.h"


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
    uint8_t buffer[128] = {0};
    
    // Get executor status
    bncurl_executor_status_t status = bncurl_executor_get_status();
    const char *status_str;
    
    switch (status) {
        case BNCURL_EXECUTOR_IDLE:
            status_str = "IDLE";
            break;
        case BNCURL_EXECUTOR_QUEUED:
            status_str = "QUEUED";
            break;
        case BNCURL_EXECUTOR_EXECUTING:
            status_str = "EXECUTING";
            break;
        default:
            status_str = "UNKNOWN";
            break;
    }
    
    snprintf((char *)buffer, 128, "+BNCURL:%s\r\n", status_str);
    esp_at_port_write_data(buffer, strlen((char *)buffer));

    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_setup_cmd_test(uint8_t para_num)
{
    // Parse parameters first
    uint8_t parse_result = bncurl_parse_and_print_params(para_num, &bncurl_ctx->params);
    if (parse_result != ESP_AT_RESULT_CODE_OK) {
        return parse_result;
    }
    
    // Check if this is a supported method (GET, POST, HEAD)
    if (strcmp(bncurl_ctx->params.method, "GET") == 0 || 
        strcmp(bncurl_ctx->params.method, "POST") == 0 || 
        strcmp(bncurl_ctx->params.method, "HEAD") == 0) {
        
        // Try to submit the request to the executor
        if (bncurl_executor_submit_request(bncurl_ctx)) {
            // Request queued successfully - return OK
            // The actual execution happens asynchronously
            // Completion will be indicated by SEND OK/SEND ERROR messages
            return ESP_AT_RESULT_CODE_OK;
        } else {
            // Executor is busy - return ERROR
            return ESP_AT_RESULT_CODE_ERROR;
        }
    } else {
        // Unsupported method
        uint8_t buffer[64] = {0};
        snprintf((char *)buffer, 64, "ERROR: Method %s not supported\r\n", bncurl_ctx->params.method);
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_ERROR;
    }
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

static uint8_t at_bncurl_stop_query(uint8_t *cmd_name)
{
    if (!bncurl_ctx) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    uint8_t buffer[64] = {0};
    
    // Try to stop current executor operation
    bool stopped = bncurl_executor_stop_current();
    
    if (stopped) {
        snprintf((char *)buffer, 64, "+BNCURL_STOP:1\r\n");
    } else {
        snprintf((char *)buffer, 64, "+BNCURL_STOP:0\r\n");
    }
    
    esp_at_port_write_data(buffer, strlen((char *)buffer));
   
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_prog_query(uint8_t *cmd_name)
{
    if (!bncurl_ctx) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    uint64_t bytes_transferred = 0;
    uint64_t bytes_total = 0;
    
    bncurl_get_progress(bncurl_ctx, &bytes_transferred, &bytes_total);

    uint8_t buffer[128] = {0};
    // Use %u for 32-bit values to ensure compatibility
    snprintf((char *)buffer, 128, "+BNCURL_PROG:%u/%u\r\n", 
             (uint32_t)bytes_transferred, (uint32_t)bytes_total);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    
    return ESP_AT_RESULT_CODE_OK;
}

// SD Card command implementations
static uint8_t at_bnsd_mount_test(uint8_t *cmd_name)
{
    uint8_t buffer[128] = {0};
    snprintf((char *)buffer, 128, 
        "AT+BNSD_MOUNT[=<mount_point>]\r\n"
        "Mount SD card at specified mount point (default: /sdcard)\r\n");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_mount_query(uint8_t *cmd_name)
{
    uint8_t buffer[128] = {0};
    
    if (at_sd_is_mounted()) {
        const char *mount_point = at_sd_get_mount_point();
        snprintf((char *)buffer, 128, "+BNSD_MOUNT:1,\"%s\"\r\n", mount_point ? mount_point : "/sdcard");
    } else {
        snprintf((char *)buffer, 128, "+BNSD_MOUNT:0\r\n");
    }
    
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_mount_setup(uint8_t para_num)
{
    uint8_t *mount_point = NULL;
    
    if (para_num > 1) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    if (para_num == 1) {
        if (esp_at_get_para_as_str(0, &mount_point) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    
    bool success = at_sd_mount((const char *)mount_point);
    return success ? ESP_AT_RESULT_CODE_OK : ESP_AT_RESULT_CODE_ERROR;
}

static uint8_t at_bnsd_mount_exe(uint8_t *cmd_name)
{
    bool success = at_sd_mount(NULL);
    return success ? ESP_AT_RESULT_CODE_OK : ESP_AT_RESULT_CODE_ERROR;
}

static uint8_t at_bnsd_unmount_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT+BNSD_UNMOUNT\r\nUnmount SD card\r\n");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_unmount_query(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    at_sd_status_t status = at_sd_get_status();
    snprintf((char *)buffer, 64, "+BNSD_UNMOUNT:%d\r\n", (int)status);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_unmount_exe(uint8_t *cmd_name)
{
    bool success = at_sd_unmount();
    return success ? ESP_AT_RESULT_CODE_OK : ESP_AT_RESULT_CODE_ERROR;
}

static uint8_t at_bnsd_space_test(uint8_t *cmd_name)
{
    uint8_t buffer[128] = {0};
    snprintf((char *)buffer, 128, 
        "AT+BNSD_SPACE?\r\n"
        "Get SD card space information (total, used, free bytes)\r\n");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_space_query(uint8_t *cmd_name)
{
    uint8_t buffer[256] = {0};
    at_sd_info_t info;
    
    if (!at_sd_get_space_info(&info)) {
        snprintf((char *)buffer, 256, "+BNSD_SPACE:ERROR\r\n");
    } else {
        // Return values in MB for readability
        uint32_t total_mb = (uint32_t)(info.total_bytes / (1024 * 1024));
        uint32_t used_mb = (uint32_t)(info.used_bytes / (1024 * 1024));
        uint32_t free_mb = (uint32_t)(info.free_bytes / (1024 * 1024));
        
        snprintf((char *)buffer, 256, "+BNSD_SPACE:%u,%u,%u\r\n", total_mb, used_mb, free_mb);
    }
    
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_format_test(uint8_t *cmd_name)
{
    uint8_t buffer[128] = {0};
    snprintf((char *)buffer, 128, 
        "AT+BNSD_FORMAT\r\n"
        "Format SD card with FAT32 filesystem\r\n"
        "WARNING: This will erase all data on the SD card!\r\n");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_format_query(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "+BNSD_FORMAT:%s\r\n", 
             at_sd_is_mounted() ? "READY" : "NO_CARD");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_format_exe(uint8_t *cmd_name)
{
    bool success = at_sd_format();
    return success ? ESP_AT_RESULT_CODE_OK : ESP_AT_RESULT_CODE_ERROR;
}

static const esp_at_cmd_struct at_custom_cmd[] = {
    {"+BNCURL", at_test_cmd_test, at_query_cmd_test, at_setup_cmd_test, at_exe_cmd_test},
    {"+BNCURL_TIMEOUT", at_bncurl_timeout_test, at_bncurl_timeout_query, at_bncurl_timeout_setup, NULL},
    {"+BNCURL_STOP", NULL, at_bncurl_stop_query, NULL, NULL},
    {"+BNCURL_PROG", NULL, at_bncurl_prog_query, NULL, NULL},
    {"+BNSD_MOUNT", at_bnsd_mount_test, at_bnsd_mount_query, at_bnsd_mount_setup, at_bnsd_mount_exe},
    {"+BNSD_UNMOUNT", at_bnsd_unmount_test, at_bnsd_unmount_query, NULL, at_bnsd_unmount_exe},
    {"+BNSD_SPACE", at_bnsd_space_test, at_bnsd_space_query, NULL, NULL},
    {"+BNSD_FORMAT", at_bnsd_format_test, at_bnsd_format_query, NULL, at_bnsd_format_exe},
};

bool esp_at_custom_cmd_register(void)
{
    // Initialize the executor first
    if (!bncurl_executor_init()) {
        return false;
    }
    
    // Initialize SD card module
    if (!at_sd_init()) {
        bncurl_executor_deinit();
        return false;
    }
    
    bncurl_ctx = calloc(1, sizeof(bncurl_context_t));
    if(!bncurl_ctx) {
        bncurl_executor_deinit();
        return false;
    }
    if(!bncurl_init(bncurl_ctx)) {
        free(bncurl_ctx);
        bncurl_ctx = NULL;
        bncurl_executor_deinit();
        return false;
    }
    return esp_at_custom_cmd_array_regist(at_custom_cmd, sizeof(at_custom_cmd) / sizeof(esp_at_cmd_struct));
}

ESP_AT_CMD_SET_INIT_FN(esp_at_custom_cmd_register, 1);