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
#include "util.h"
#include "bnwps.h"
#include "bncert.h"
#include "bncert_manager.h"
#include "bnwebradio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"


static bncurl_context_t *bncurl_ctx = NULL;


static const char *TAG = "AT_BONES";

// UART data collection timeout (30 seconds)
#define UART_DATA_COLLECTION_TIMEOUT_MS 30000

// Semaphore for UART data collection synchronization
static SemaphoreHandle_t s_uart_data_sync_sema = NULL;

// Callback for UART data collection
static void uart_data_wait_callback(void)
{
    if (s_uart_data_sync_sema) {
        xSemaphoreGive(s_uart_data_sync_sema);
    }
}

/**
 * @brief Collect data from UART with timeout for numeric -du parameter
 * 
 * @param expected_bytes Number of bytes to collect
 * @param collected_data Pointer to store collected data (caller must free)
 * @param collected_size Pointer to store actual collected size
 * @return true if all bytes collected successfully, false on timeout or error
 */
static bool collect_uart_data(size_t expected_bytes, char **collected_data, size_t *collected_size)
{
    if (expected_bytes == 0) {
        // Special case: 0 bytes - no data collection needed
        *collected_data = NULL;
        *collected_size = 0;
        ESP_LOGI(TAG, "No UART data collection needed (0 bytes expected)");
        return true;
    }
    
    // Allocate buffer for collected data
    *collected_data = malloc(expected_bytes + 1); // +1 for null terminator if needed
    if (!*collected_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for UART data collection");
        return false;
    }
    
    // Create semaphore for data collection synchronization
    if (!s_uart_data_sync_sema) {
        s_uart_data_sync_sema = xSemaphoreCreateBinary();
        if (!s_uart_data_sync_sema) {
            ESP_LOGE(TAG, "Failed to create UART data sync semaphore");
            free(*collected_data);
            *collected_data = NULL;
            return false;
        }
    }
    
    *collected_size = 0;
    uint32_t timeout_ticks = pdMS_TO_TICKS(UART_DATA_COLLECTION_TIMEOUT_MS);
    
    ESP_LOGI(TAG, "Collecting %u bytes from UART (timeout: %d ms)", (unsigned int)expected_bytes, UART_DATA_COLLECTION_TIMEOUT_MS);
    
    // Enter specific mode for UART data collection
    esp_at_port_enter_specific(uart_data_wait_callback);
    
    // Show prompt
    esp_at_port_write_data((uint8_t *)">", 1);
    
    // Collect data using ESP-AT framework
    while (*collected_size < expected_bytes) {
        if (xSemaphoreTake(s_uart_data_sync_sema, timeout_ticks) == pdTRUE) {
            // Read available data
            size_t bytes_to_read = expected_bytes - *collected_size;
            size_t bytes_read = esp_at_port_read_data((uint8_t *)(*collected_data + *collected_size), bytes_to_read);
            *collected_size += bytes_read;
            
            ESP_LOGD(TAG, "Read %u bytes, total collected: %u/%u", 
                     (unsigned int)bytes_read, (unsigned int)*collected_size, (unsigned int)expected_bytes);
            
            if (*collected_size >= expected_bytes) {
                break;
            }
        } else {
            // Timeout occurred
            ESP_LOGW(TAG, "UART data collection timeout after %d ms", UART_DATA_COLLECTION_TIMEOUT_MS);
            printf("ERROR: Timeout waiting for %u bytes (collected %u)\r\n", (unsigned int)expected_bytes, (unsigned int)*collected_size);
            esp_at_port_exit_specific();
            vSemaphoreDelete(s_uart_data_sync_sema);
            s_uart_data_sync_sema = NULL;
            free(*collected_data);
            *collected_data = NULL;
            return false;
        }
    }
    
    // Exit specific mode
    esp_at_port_exit_specific();
    
    // Clean up semaphore
    vSemaphoreDelete(s_uart_data_sync_sema);
    s_uart_data_sync_sema = NULL;
    
    // Null-terminate for safety (doesn't count toward data size)
    (*collected_data)[*collected_size] = '\0';
    
    ESP_LOGI(TAG, "Successfully collected %u bytes from UART", (unsigned int)*collected_size);
    return true;
}

static uint8_t at_test_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[512] = {0};
    snprintf((char *)buffer, 512, 
        "AT+BNCURL=<method>,<url>[,<options>]\r\n"
        "HTTP/HTTPS client with libcurl support\r\n"
        "\r\n"
        "Methods: GET, POST, HEAD\r\n"
        "Options:\r\n"
        "  -H \"Header: Value\"  Custom HTTP header\r\n"
        "  -du <bytes|@file>   Upload data (POST only)\r\n"
        "  -dd <@file>         Download to file\r\n"
        "  -c <@file>          Save cookies to file\r\n"
        "  -b <@file>          Send cookies from file\r\n"
        "  -r <start-end>      Range request (GET only, optional with -dd)\r\n"
        "  -v                  Verbose debug output\r\n"
        "\r\n"
        "Range Downloads:\r\n"
        "  -r \"0-2097151\"       Download bytes 0-2097151 (to file or UART)\r\n"
        "  -r \"2097152-4194303\" Download next 2MB chunk (to file or UART)\r\n"
        "  With -dd: appends to file | Without -dd: streams to UART\r\n"
        "\r\n"
        "Examples:\r\n"
        "  AT+BNCURL=\"GET\",\"http://example.com/file.mp3\",\"-dd\",\"@file.mp3\",\"-r\",\"0-2097151\"\r\n"
        "  AT+BNCURL=\"GET\",\"http://example.com/file.mp3\",\"-r\",\"0-2097151\"\r\n");
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
        bncurl_params_cleanup(&bncurl_ctx->params);
        return parse_result;
    }
    
    // Check if this is a supported method (GET, POST, HEAD)
    if (strcmp(bncurl_ctx->params.method, "GET") == 0 || 
        strcmp(bncurl_ctx->params.method, "POST") == 0 || 
        strcmp(bncurl_ctx->params.method, "HEAD") == 0) {
        
        // Check if we need to collect data from UART first (numeric -du)
        if (bncurl_ctx->params.is_numeric_upload) {
            // For numeric upload, collect data immediately before sending OK
            char *collected_data = NULL;
            size_t collected_size = 0;
            
            bool collection_success = collect_uart_data(
                bncurl_ctx->params.upload_bytes_expected, 
                &collected_data, 
                &collected_size
            );
            
            if (!collection_success) {
                ESP_LOGE(TAG, "UART data collection failed");
                bncurl_params_cleanup(&bncurl_ctx->params);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            // Store collected data in context
            bncurl_ctx->params.collected_data = collected_data;
            bncurl_ctx->params.collected_data_size = collected_size;
            
            ESP_LOGI(TAG, "Data collection successful, submitting request to executor");
        }
        
        // Try to submit the request to the executor
        if (bncurl_executor_submit_request(bncurl_ctx)) {
            // Request queued successfully - return OK
            // The actual execution happens asynchronously
            // Completion will be indicated by SEND OK/SEND ERROR messages
            return ESP_AT_RESULT_CODE_OK;
        } else {
            // Executor is busy - return ERROR
            bncurl_params_cleanup(&bncurl_ctx->params);
            return ESP_AT_RESULT_CODE_ERROR;
        }
    } else {
        // Unsupported method
        uint8_t buffer[64] = {0};
        snprintf((char *)buffer, 64, "ERROR: Method %s not supported\r\n", bncurl_ctx->params.method);
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        bncurl_params_cleanup(&bncurl_ctx->params);
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
        "Get SD card space information in format: +BNSD_SPACE:total_bytes/used_bytes\r\n"
        "Note: used_bytes includes filesystem overhead and user data\r\n");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_space_query(uint8_t *cmd_name)
{
    uint8_t buffer[512] = {0};  // Increased buffer size for uint64 strings
    at_sd_info_t info;
    
    if (!at_sd_get_space_info(&info)) {
        snprintf((char *)buffer, sizeof(buffer), "+BNSD_SPACE:ERROR\r\n");
    } else {
        // Convert uint64_t values to strings using util function
        char total_str[32];
        char used_str[32];
        
        int total_len = uint64_to_string(info.total_bytes, total_str, sizeof(total_str));
        int used_len = uint64_to_string(info.used_bytes, used_str, sizeof(used_str));
        
        if (total_len > 0 && used_len > 0) {
            // Format: +BNSD_SPACE:total_bytes/used_bytes
            snprintf((char *)buffer, sizeof(buffer), "+BNSD_SPACE:%s/%s\r\n", total_str, used_str);
        } else {
            // Fallback if string conversion fails
            snprintf((char *)buffer, sizeof(buffer), "+BNSD_SPACE:ERROR\r\n");
        }
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

// WPS Command Handlers
static uint8_t at_bnwps_test(uint8_t *cmd_name)
{
    uint8_t buffer[512] = {0};
    snprintf((char *)buffer, 512,
        "+BNWPS:<t>\r\n"
        "Set WPS timeout in seconds (1-300, 0=cancel)\r\n"
        "\r\n"
        "AT+BNWPS?\r\n"
        "Query WPS status\r\n"
        "\r\n"
        "Examples:\r\n"
        "  AT+BNWPS=60      Start WPS for 60 seconds\r\n"
        "  AT+BNWPS=0       Cancel WPS operation\r\n"
        "  AT+BNWPS?        Check current WPS status\r\n"
        "\r\n"
        "Response on success:\r\n"
        "  +CWJAP:\"<ssid>\",\"<bssid>\",<channel>,<rssi>,<pci_en>,<reconn_interval>,<listen_interval>,<scan_mode>,<pmf>\r\n"
        "  OK\r\n"
        "\r\n"
        "Response on error:\r\n"
        "  +CWJAP:<error_code>\r\n"
        "  ERROR\r\n");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnwps_query(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    
    bnwps_status_t status = bnwps_get_status();
    int status_value = (status == BNWPS_STATUS_ACTIVE) ? 1 : 0;
    
    snprintf((char *)buffer, 64, "+BNWPS:%d\r\n", status_value);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnwps_setup(uint8_t para_num)
{
    int32_t timeout_seconds;
    
    // Parse the timeout parameter
    if (esp_at_get_para_as_digit(0, &timeout_seconds) != ESP_AT_PARA_PARSE_RESULT_OK) {
        printf("ERROR: Invalid timeout parameter\n");
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Validate range
    if (timeout_seconds < 0 || timeout_seconds > BNWPS_MAX_TIMEOUT_SECONDS) {
        printf("ERROR: Timeout must be 0-%u seconds\n", BNWPS_MAX_TIMEOUT_SECONDS);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Initialize WPS if not already done
    if (!bnwps_init()) {
        printf("ERROR: Failed to initialize WPS\n");
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Start or cancel WPS operation
    bool success = bnwps_start((uint16_t)timeout_seconds);
    if (!success) {
        printf("ERROR: Failed to start WPS operation\n");
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // For cancel operation (timeout_seconds = 0), send immediate response
    if (timeout_seconds == 0) {
        uint8_t buffer[32] = {0};
        snprintf((char *)buffer, 32, "+BNWPS:0\r\n");
        esp_at_port_write_data(buffer, strlen((char *)buffer));
    }
    
    return ESP_AT_RESULT_CODE_OK;
}

// Certificate Command Handlers
static uint8_t at_bnflash_cert_test(uint8_t *cmd_name)
{
    uint8_t buffer[512] = {0};
    snprintf((char *)buffer, 512,
        "+BNFLASH_CERT:<flash_address>,<data_source>\r\n"
        "Flash certificate to specified flash address\r\n"
        "\r\n"
        "Parameters:\r\n"
        "  <flash_address>  Absolute flash memory address (hex: 0xNNNNNN)\r\n"
        "  <data_source>    File path (@/path/file) or byte count (NNNN)\r\n"
        "\r\n"
        "Examples:\r\n"
        "  AT+BNFLASH_CERT=0x2A000,@/certs/server_key.bin\r\n"
        "  AT+BNFLASH_CERT=0x2A000,1400\r\n"
        "\r\n"
        "File mode: Certificate read from SD card file\r\n"
        "UART mode: System prompts with '>' for certificate data\r\n"
        "\r\n"
        "Uses dedicated certificate partition for safe storage\r\n"
        "Maximum data size: 65536 bytes\r\n"
        "Address must be 4-byte aligned\r\n");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

// Global certificate parameters for sharing between setup and execution phases
static bncert_params_t g_cert_params;

static uint8_t at_bnflash_cert_setup(uint8_t para_num)
{
    // Parse parameters - we expect exactly 2 parameters: flash_address and data_source
    uint8_t parse_result = bncert_parse_params(para_num, &g_cert_params);
    if (parse_result != ESP_AT_RESULT_CODE_OK) {
        return parse_result;
    }
    
    // Initialize certificate subsystem if not already done
    if (!bncert_init()) {
        esp_at_port_write_data((uint8_t *)"ERROR: Failed to initialize certificate flashing\r\n", 51);
        bncert_cleanup_params(&g_cert_params);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // Handle UART data collection if needed
    if (g_cert_params.source_type == BNCERT_SOURCE_UART) {
        if (!bncert_collect_uart_data(&g_cert_params)) {
            esp_at_port_write_data((uint8_t *)"ERROR: Failed to collect certificate data from UART\r\n", 54);
            bncert_cleanup_params(&g_cert_params);
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    
    // Execute certificate flashing (for both file and UART sources)
    bncert_result_t flash_result = bncert_flash_certificate(&g_cert_params);
    
    // Send result response
    if (flash_result == BNCERT_RESULT_OK) {
        // Send success response
        char response[128];
        size_t data_size = (g_cert_params.source_type == BNCERT_SOURCE_UART) ? 
                          g_cert_params.collected_size : g_cert_params.data_size;
        
        snprintf(response, sizeof(response), 
                "+BNFLASH_CERT:OK,0x%08X,%u\r\n", 
                (unsigned int)g_cert_params.flash_address,
                (unsigned int)data_size);
        esp_at_port_write_data((uint8_t *)response, strlen(response));
        
        // Register certificate with certificate manager
        if (bncert_manager_init()) {
            if (!bncert_manager_register(g_cert_params.flash_address, data_size)) {
                ESP_LOGW("AT_BONES", "Failed to register certificate with manager");
            } else {
                ESP_LOGI("AT_BONES", "Registered certificate with manager at 0x%08X", 
                         (unsigned int)g_cert_params.flash_address);
            }
        } else {
            ESP_LOGW("AT_BONES", "Certificate manager not available for registration");
        }
        
        bncert_cleanup_params(&g_cert_params);
        return ESP_AT_RESULT_CODE_OK;
    } else {
        // Send error response
        char error_response[128];
        snprintf(error_response, sizeof(error_response),
                "ERROR: Certificate flashing failed: %s\r\n",
                bncert_get_result_string(flash_result));
        esp_at_port_write_data((uint8_t *)error_response, strlen(error_response));
        
        bncert_cleanup_params(&g_cert_params);
        return ESP_AT_RESULT_CODE_ERROR;
    }
}

// Certificate list command - lists all certificates stored in partition
static uint8_t at_bncert_list_test(uint8_t *cmd_name)
{
    esp_at_port_write_data((uint8_t *)"+BNCERT_LIST: List certificates in partition\r\n", 47);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncert_list_query(uint8_t *cmd_name)
{
    // Initialize certificate manager
    if (!bncert_manager_init()) {
        printf("ERROR: Certificate manager initialization failed\n");
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // List all certificates
    bncert_manager_list_certificates();
    
    return ESP_AT_RESULT_CODE_OK;
}

// Certificate addresses command - lists all valid certificate storage addresses
static uint8_t at_bncert_addr_test(uint8_t *cmd_name)
{
    esp_at_port_write_data((uint8_t *)"+BNCERT_ADDR: List valid certificate storage addresses\r\n", 57);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncert_addr_query(uint8_t *cmd_name)
{
    // Initialize certificate subsystem
    if (!bncert_init()) {
        printf("ERROR: Certificate subsystem initialization failed\n");
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    // List all valid addresses
    bncert_list_valid_addresses();
    
    return ESP_AT_RESULT_CODE_OK;
}

// Certificate clear command - clears (erases) certificate at specific address
static uint8_t at_bncert_clear_test(uint8_t *cmd_name)
{
    esp_at_port_write_data((uint8_t *)"+BNCERT_CLEAR:<address>\r\n", 25);
    esp_at_port_write_data((uint8_t *)"Clear certificate at specified flash address\r\n", 47);
    esp_at_port_write_data((uint8_t *)"Address must be 4KB aligned and within certificate partition\r\n", 62);
    esp_at_port_write_data((uint8_t *)"Example: AT+BNCERT_CLEAR=0x380000\r\n", 36);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncert_clear_setup(uint8_t para_num)
{
    // Need exactly 1 parameter: address
    if (para_num != 1) {
        esp_at_port_write_data((uint8_t *)"ERROR: AT+BNCERT_CLEAR requires exactly 1 parameter: <address>\r\n", 66);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    // Parse address parameter
    int32_t addr_value;
    if (esp_at_get_para_as_digit(0, &addr_value) != ESP_AT_PARA_PARSE_RESULT_OK) {
        esp_at_port_write_data((uint8_t *)"ERROR: Invalid address parameter\r\n", 34);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    uint32_t address = (uint32_t)addr_value;

    // Initialize certificate manager
    if (!bncert_manager_init()) {
        esp_at_port_write_data((uint8_t *)"ERROR: Certificate manager initialization failed\r\n", 50);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    // Clear the certificate
    if (bncert_manager_clear_cert(address)) {
        char response[64];
        int len = snprintf(response, sizeof(response), "+BNCERT_CLEAR:OK,0x%08X\r\n", (unsigned int)address);
        esp_at_port_write_data((uint8_t *)response, len);
        return ESP_AT_RESULT_CODE_OK;
    } else {
        char response[64];
        int len = snprintf(response, sizeof(response), "+BNCERT_CLEAR:ERROR,0x%08X\r\n", (unsigned int)address);
        esp_at_port_write_data((uint8_t *)response, len);
        return ESP_AT_RESULT_CODE_ERROR;
    }
}

// Certificate flash command (alias for BNFLASH_CERT with more intuitive name)
static uint8_t at_bncert_flash_test(uint8_t *cmd_name)
{
    esp_at_port_write_data((uint8_t *)"+BNCERT_FLASH:<flash_address>,<data_source>\r\n", 46);
    esp_at_port_write_data((uint8_t *)"Flash certificate data to partition\r\n", 38);
    esp_at_port_write_data((uint8_t *)"Parameters:\r\n", 13);
    esp_at_port_write_data((uint8_t *)"  flash_address: 4KB-aligned address in certificate partition\r\n", 64);
    esp_at_port_write_data((uint8_t *)"  data_source: @/path/to/file (SD card) or byte_count (UART)\r\n", 63);
    esp_at_port_write_data((uint8_t *)"Examples:\r\n", 11);
    esp_at_port_write_data((uint8_t *)"  AT+BNCERT_FLASH=0x380000,@/certs/certificate.pem\r\n", 52);
    esp_at_port_write_data((uint8_t *)"  AT+BNCERT_FLASH=0x381000,@/certs/private_key.key\r\n", 52);
    esp_at_port_write_data((uint8_t *)"  AT+BNCERT_FLASH=0x382000,1024\r\n", 33);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncert_flash_setup(uint8_t para_num)
{
    // This is just an alias for the existing BNFLASH_CERT command
    return at_bnflash_cert_setup(para_num);
}

// Web Radio command handlers
static uint8_t at_bnweb_radio_test(uint8_t *cmd_name)
{
    esp_at_port_write_data((uint8_t *)"+BNWEB_RADIO:(0,1)\r\n", strlen("+BNWEB_RADIO:(0,1)\r\n"));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnweb_radio_query(uint8_t *cmd_name)
{
    webradio_state_t state = bnwebradio_get_state();
    size_t bytes_streamed = 0;
    uint32_t duration_ms = 0;
    
    char response[128];
    if (bnwebradio_is_active() && bnwebradio_get_stats(&bytes_streamed, &duration_ms)) {
        snprintf(response, sizeof(response), "+BNWEB_RADIO:1,%zu,%u\r\n", 
                bytes_streamed, duration_ms);
    } else {
        snprintf(response, sizeof(response), "+BNWEB_RADIO:0\r\n");
    }
    
    esp_at_port_write_data((uint8_t *)response, strlen(response));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnweb_radio_setup(uint8_t para_num)
{
    uint8_t *url_param = NULL;
    int32_t enable = 0;
    
    // Parse enable parameter (0 or 1)
    if (esp_at_get_para_as_digit(0, &enable) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    if (enable == 0) {
        // Stop web radio
        if (!bnwebradio_stop()) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
        return ESP_AT_RESULT_CODE_OK;
    } else if (enable == 1) {
        // Start web radio - need URL parameter
        if (esp_at_get_para_as_str(1, &url_param) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        // Validate URL
        if (!url_param || strlen((char *)url_param) == 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        // Start web radio streaming
        if (!bnwebradio_start((char *)url_param)) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        return ESP_AT_RESULT_CODE_OK;
    }
    
    return ESP_AT_RESULT_CODE_ERROR;
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
    {"+BNWPS", at_bnwps_test, at_bnwps_query, at_bnwps_setup, NULL},
    {"+BNFLASH_CERT", at_bnflash_cert_test, NULL, at_bnflash_cert_setup, NULL},
    {"+BNCERT_FLASH", at_bncert_flash_test, NULL, at_bncert_flash_setup, NULL},
    {"+BNCERT_LIST", at_bncert_list_test, at_bncert_list_query, NULL, NULL},
    {"+BNCERT_ADDR", at_bncert_addr_test, at_bncert_addr_query, NULL, NULL},
    {"+BNCERT_CLEAR", at_bncert_clear_test, NULL, at_bncert_clear_setup, NULL},
    {"+BNWEB_RADIO", at_bnweb_radio_test, at_bnweb_radio_query, at_bnweb_radio_setup, NULL},
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
    
    // Initialize WPS subsystem
    if (!bnwps_init()) {
        // WPS initialization failure is not fatal, log warning and continue
        ESP_LOGW("AT_BONES", "Failed to initialize WPS subsystem");
    }
    
    // Initialize certificate flashing subsystem
    if (!bncert_init()) {
        // Certificate initialization failure is not fatal, log warning and continue
        ESP_LOGW("AT_BONES", "Failed to initialize certificate flashing subsystem");
    }
    
    // Initialize certificate manager subsystem
    if (!bncert_manager_init()) {
        // Certificate manager initialization failure is not fatal, log warning and continue
        ESP_LOGW("AT_BONES", "Failed to initialize certificate manager subsystem");
    }
    
    // Initialize web radio subsystem
    if (!bnwebradio_init()) {
        // Web radio initialization failure is not fatal, log warning and continue
        ESP_LOGW("AT_BONES", "Failed to initialize web radio subsystem");
    }
    
    return esp_at_custom_cmd_array_regist(at_custom_cmd, sizeof(at_custom_cmd) / sizeof(esp_at_cmd_struct));
}

ESP_AT_CMD_SET_INIT_FN(esp_at_custom_cmd_register, 1);