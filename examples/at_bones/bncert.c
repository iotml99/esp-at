/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bncert.h"
#include "bncert_manager.h"
#include "bncurl_params.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_at.h"
#include "util.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

static const char *TAG = "BNCERT";

// Function to validate that file path starts with @ prefix for SD card
static bool validate_cert_file_path_prefix(const char *file_path)
{
    if (!file_path || strlen(file_path) == 0) {
        esp_at_port_write_data((uint8_t *)"ERROR: Empty file path for certificate\r\n", 41);
        return false;
    }
    
    if (file_path[0] != '@') {
        ESP_LOGE(TAG, "Invalid certificate file path: %s (must start with @)", file_path);
        char error_msg[120];
        int len = snprintf(error_msg, sizeof(error_msg), 
                          "ERROR: Certificate file path must start with @ (SD card prefix): %s\r\n", 
                          file_path);
        esp_at_port_write_data((uint8_t *)error_msg, len);
        return false;
    }
    
    return true;
}

// Certificate subsystem state
static bool s_bncert_initialized = false;
static const esp_partition_t *s_cert_partition = NULL;

bool bncert_init(void)
{
    if (s_bncert_initialized) {
        ESP_LOGW(TAG, "Certificate flashing already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing certificate flashing subsystem");

    // Find the certificate partition
    s_cert_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, NULL);
    if (!s_cert_partition) {
        ESP_LOGE(TAG, "Certificate partition not found. Please add 'certs' partition to partition table.");
        return false;
    }

    ESP_LOGI(TAG, "Found certificate partition: address=0x%08X, size=%u bytes", 
             (unsigned int)s_cert_partition->address, (unsigned int)s_cert_partition->size);

    // Print valid address information (only to debug log, not console)
    ESP_LOGI(TAG, "Certificate partition found at 0x%08X (%u bytes)", 
           (unsigned int)s_cert_partition->address, (unsigned int)s_cert_partition->size);

    // Initialize certificate manager for automatic discovery
    if (!bncert_manager_init()) {
        ESP_LOGW(TAG, "Certificate manager initialization failed, but basic flashing will still work");
    }

    s_bncert_initialized = true;
    ESP_LOGI(TAG, "Certificate flashing subsystem initialized");
    return true;
}

void bncert_deinit(void)
{
    if (!s_bncert_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing certificate flashing subsystem");
    bncert_manager_deinit();
    s_cert_partition = NULL;
    s_bncert_initialized = false;
}

uint8_t bncert_parse_params(uint8_t para_num, bncert_params_t *params)
{
    if (!params) {
        ESP_LOGE(TAG, "Invalid parameters structure");
        return ESP_AT_RESULT_CODE_ERROR;
    }

    // Initialize parameters
    memset(params, 0, sizeof(bncert_params_t));

    // Need exactly 2 parameters: flash_address and data_source
    if (para_num != 2) {
        esp_at_port_write_data((uint8_t *)"ERROR: AT+BNCERT_FLASH requires exactly 2 parameters: <flash_address>,<data_source>\r\n", 85);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    // Parse flash address (parameter 0)
    int32_t addr_value;
    if (esp_at_get_para_as_digit(0, &addr_value) != ESP_AT_PARA_PARSE_RESULT_OK) {
        esp_at_port_write_data((uint8_t *)"ERROR: Invalid flash address parameter\r\n", 40);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    params->flash_address = (uint32_t)addr_value;

    // Parse data source (parameter 1) - try digit first, then string
    int32_t digit_value;
    uint8_t *data_source_str = NULL;
    
    // Try parsing as digit first (for unquoted numbers like 1674)
    if (esp_at_get_para_as_digit(1, &digit_value) == ESP_AT_PARA_PARSE_RESULT_OK) {
        // Successfully parsed as digit - this is UART data source
        if (digit_value <= 0 || digit_value > BNCERT_MAX_DATA_SIZE) {
            char error_msg[120];
            int len = snprintf(error_msg, sizeof(error_msg), 
                              "ERROR: Invalid data size: %ld bytes (must be 1-%u, max 4KB)\r\n", 
                              (long)digit_value, BNCERT_MAX_DATA_SIZE);
            esp_at_port_write_data((uint8_t *)error_msg, len);
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        params->source_type = BNCERT_SOURCE_UART;
        params->data_size = (size_t)digit_value;
        ESP_LOGI(TAG, "Certificate source: UART (%u bytes)", (unsigned int)params->data_size);
        
    } else if (esp_at_get_para_as_str(1, &data_source_str) == ESP_AT_PARA_PARSE_RESULT_OK) {
        // Parse as string (for quoted file paths like "@/certificate.pem")
        
        if (data_source_str == NULL || strlen((char *)data_source_str) == 0) {
            esp_at_port_write_data((uint8_t *)"ERROR: Empty data source parameter\r\n", 37);
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        if (data_source_str[0] == '@') {
            // File source - validate the @ prefix
            if (!validate_cert_file_path_prefix((char *)data_source_str)) {
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            params->source_type = BNCERT_SOURCE_FILE;
            
            if (strlen((char *)data_source_str) > BNCERT_MAX_FILE_PATH_LENGTH) {
                char error_msg[80];
                int len = snprintf(error_msg, sizeof(error_msg), 
                                  "ERROR: File path too long (max %d characters)\r\n", 
                                  BNCERT_MAX_FILE_PATH_LENGTH);
                esp_at_port_write_data((uint8_t *)error_msg, len);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            strncpy(params->file_path, (char *)data_source_str, BNCERT_MAX_FILE_PATH_LENGTH);
            params->file_path[BNCERT_MAX_FILE_PATH_LENGTH] = '\0';
            
            // Normalize the file path (remove @ and prepend mount point)
            normalize_path_with_mount_point(params->file_path, BNCERT_MAX_FILE_PATH_LENGTH);
            
            ESP_LOGI(TAG, "Certificate source: file %s", params->file_path);
        } else {
            // Check if it's a quoted numeric string (fallback for quoted numbers)
            char *endptr;
            long data_size = strtol((char *)data_source_str, &endptr, 10);
            
            if (*endptr != '\0' || data_size <= 0 || data_size > BNCERT_MAX_DATA_SIZE) {
                // Not a valid number and doesn't start with @ - invalid
                char error_msg[150];
                int len = snprintf(error_msg, sizeof(error_msg), 
                                  "ERROR: Invalid data source '%s' (must be unquoted number 1-%u or quoted file path starting with @)\r\n", 
                                  (char *)data_source_str, BNCERT_MAX_DATA_SIZE);
                esp_at_port_write_data((uint8_t *)error_msg, len);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            // Valid quoted UART data source
            params->source_type = BNCERT_SOURCE_UART;
            params->data_size = (size_t)data_size;
            ESP_LOGI(TAG, "Certificate source: UART (%u bytes from quoted string)", (unsigned int)params->data_size);
        }
    } else {
        esp_at_port_write_data((uint8_t *)"ERROR: Failed to parse data source parameter\r\n", 47);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    // Validate flash address
    if (!bncert_validate_flash_address(params->flash_address, 
                                       params->source_type == BNCERT_SOURCE_UART ? 
                                       params->data_size : BNCERT_MAX_DATA_SIZE)) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    ESP_LOGI(TAG, "Parsed certificate parameters: addr=0x%08X, source=%s", 
             (unsigned int)params->flash_address,
             params->source_type == BNCERT_SOURCE_FILE ? "file" : "uart");

    return ESP_AT_RESULT_CODE_OK;
}

bool bncert_validate_flash_address(uint32_t address, size_t size)
{
    if (!s_cert_partition) {
        ESP_LOGE(TAG, "Certificate partition not initialized");
        return false;
    }

    // Get partition boundaries
    uint32_t partition_start = s_cert_partition->address;
    uint32_t partition_end = s_cert_partition->address + s_cert_partition->size;
    
    // Check 4KB alignment
    if (address % 0x1000 != 0) {
        ESP_LOGE(TAG, "Address 0x%08X not 4KB aligned", (unsigned int)address);
        char error_msg[60];
        int len = snprintf(error_msg, sizeof(error_msg), 
                          "ERROR: Address must be 4KB aligned\r\n");
        esp_at_port_write_data((uint8_t *)error_msg, len);
        return false;
    }

    // Check if address is within certificate partition bounds
    if (address < partition_start || address >= partition_end) {
        ESP_LOGE(TAG, "Address 0x%08X outside certificate partition bounds", (unsigned int)address);
        char error_msg[80];
        int len = snprintf(error_msg, sizeof(error_msg), 
                          "ERROR: Address outside certificate partition\r\n");
        esp_at_port_write_data((uint8_t *)error_msg, len);
        return false;
    }

    // Check that address + size doesn't exceed partition end
    if (address + size > partition_end) {
        ESP_LOGE(TAG, "Certificate data would exceed partition boundary");
        char error_msg[80];
        int len = snprintf(error_msg, sizeof(error_msg), 
                          "ERROR: Certificate data exceeds partition boundary\r\n");
        esp_at_port_write_data((uint8_t *)error_msg, len);
        return false;
    }

    // Check size is reasonable
    if (size == 0 || size > BNCERT_MAX_DATA_SIZE) {
        ESP_LOGE(TAG, "Invalid certificate size: %u bytes (must be 1-%u, max 4KB)", 
                 (unsigned int)size, BNCERT_MAX_DATA_SIZE);
        char error_msg[80];
        int len = snprintf(error_msg, sizeof(error_msg), 
                          "ERROR: Certificate size exceeds 4KB limit\r\n");
        esp_at_port_write_data((uint8_t *)error_msg, len);
        return false;
    }

    ESP_LOGI(TAG, "Address 0x%08X validated for %u bytes", (unsigned int)address, (unsigned int)size);
    return true;
}

// Static semaphore for UART data synchronization (similar to AT user commands)
static SemaphoreHandle_t s_bncert_data_sync_sema = NULL;

// Callback function for UART data arrival (similar to at_user_wait_data_cb)
static void bncert_wait_data_cb(void)
{
    if (s_bncert_data_sync_sema) {
        xSemaphoreGive(s_bncert_data_sync_sema);
    }
}

bool bncert_collect_uart_data(bncert_params_t *params)
{
    if (!params || params->source_type != BNCERT_SOURCE_UART) {
        ESP_LOGE(TAG, "Invalid parameters for UART data collection");
        return false;
    }

    if (params->data_size == 0 || params->data_size > BNCERT_MAX_DATA_SIZE) {
        ESP_LOGE(TAG, "Invalid data size: %u bytes (must be 1-%u)", 
                 (unsigned int)params->data_size, BNCERT_MAX_DATA_SIZE);
        char error_msg[80];
        int len = snprintf(error_msg, sizeof(error_msg), 
                          "ERROR: Data size %u exceeds 4KB limit (%u bytes)\r\n", 
                          (unsigned int)params->data_size, BNCERT_MAX_DATA_SIZE);
        esp_at_port_write_data((uint8_t *)error_msg, len);
        return false;
    }

    // Allocate 4KB buffer regardless of requested size for safety
    params->uart_data = malloc(BNCERT_MAX_DATA_SIZE);
    if (!params->uart_data) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes for UART data buffer", BNCERT_MAX_DATA_SIZE);
        char error_msg[80];
        int len = snprintf(error_msg, sizeof(error_msg), 
                          "ERROR: Failed to allocate 4KB buffer for certificate data\r\n");
        esp_at_port_write_data((uint8_t *)error_msg, len);
        return false;
    }

    // Clear the entire buffer
    memset(params->uart_data, 0, BNCERT_MAX_DATA_SIZE);

    ESP_LOGI(TAG, "Collecting %u bytes from UART using AT framework pattern", 
             (unsigned int)params->data_size);

    // Create semaphore for data synchronization (like AT user commands)
    s_bncert_data_sync_sema = xSemaphoreCreateBinary();
    if (!s_bncert_data_sync_sema) {
        ESP_LOGE(TAG, "Failed to create data synchronization semaphore");
        free(params->uart_data);
        params->uart_data = NULL;
        return false;
    }

    // Enter specific mode with our callback (like AT user commands do)
    esp_at_port_enter_specific(bncert_wait_data_cb);
    
    // Send just the ">" prompt manually (since there's no prompt-only result code)
    const char *prompt = ">";
    esp_at_port_write_data((uint8_t *)prompt, strlen(prompt));

    // Collect data using the AT framework pattern
    int32_t bytes_received = 0;
    uint32_t timeout_ms = 30000; // 30 second timeout
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    
    // Use semaphore-based waiting like the AT user commands
    while (bytes_received < (int32_t)params->data_size) {
        // Wait for data arrival signal (with timeout)
        if (xSemaphoreTake(s_bncert_data_sync_sema, timeout_ticks) == pdTRUE) {
            // Data is available, read it
            int32_t remaining = (int32_t)params->data_size - bytes_received;
            int32_t len = esp_at_port_read_data(params->uart_data + bytes_received, remaining);
            
            if (len > 0) {
                bytes_received += len;
                ESP_LOGD(TAG, "Read %ld bytes, total: %ld/%u", 
                         (long)len, (long)bytes_received, (unsigned int)params->data_size);
                
                // Show progress every 256 bytes
                if (bytes_received % 256 == 0) {
                    ESP_LOGI(TAG, "Received %ld/%u bytes", (long)bytes_received, (unsigned int)params->data_size);
                }
            }
        } else {
            // Timeout waiting for data
            ESP_LOGE(TAG, "Timeout waiting for certificate data - received %ld/%u bytes", 
                     (long)bytes_received, (unsigned int)params->data_size);
            break;
        }
    }

    // Exit specific mode (like AT user commands do)
    esp_at_port_exit_specific();

    // Clean up semaphore
    vSemaphoreDelete(s_bncert_data_sync_sema);
    s_bncert_data_sync_sema = NULL;

    params->collected_size = (size_t)bytes_received;
    
    // Check for remaining data (this is what causes "busy p..." in AT framework)
    int32_t remaining_data = esp_at_port_get_data_length();
    if (remaining_data > 0) {
        ESP_LOGW(TAG, "Warning: %ld bytes remain in AT buffer (will cause busy message)", 
                 (long)remaining_data);
        // The AT framework will handle this and send "busy p..." automatically
    }
    
    if (bytes_received == (int32_t)params->data_size) {
        ESP_LOGI(TAG, "Successfully collected %u bytes from UART using AT framework", 
                 (unsigned int)params->collected_size);
        return true;
    } else {
        ESP_LOGW(TAG, "Partial data collection: %ld/%u bytes received", 
                 (long)bytes_received, (unsigned int)params->data_size);
        return false;
    }
}

bncert_result_t bncert_flash_certificate(bncert_params_t *params)
{
    if (!s_bncert_initialized) {
        ESP_LOGE(TAG, "Certificate flashing not initialized");
        return BNCERT_RESULT_INVALID_PARAMS;
    }

    if (!params) {
        ESP_LOGE(TAG, "Invalid parameters");
        return BNCERT_RESULT_INVALID_PARAMS;
    }

    ESP_LOGI(TAG, "Starting certificate flash operation to address 0x%08X", 
             (unsigned int)params->flash_address);

    uint8_t *data_buffer = NULL;
    size_t data_size = 0;
    bncert_result_t result = BNCERT_RESULT_OK;

    if (params->source_type == BNCERT_SOURCE_FILE) {
        // Read data from file using POSIX operations
        ESP_LOGI(TAG, "Reading certificate from file: %s", params->file_path);

        int fd = open(params->file_path, O_RDONLY);
        if (fd < 0) {
            ESP_LOGE(TAG, "Failed to open certificate file: %s (errno: %d)", params->file_path, errno);
            return BNCERT_RESULT_FILE_ERROR;
        }

        // Get file size using fstat
        struct stat file_stat;
        if (fstat(fd, &file_stat) != 0) {
            ESP_LOGE(TAG, "Failed to get file stats: %s (errno: %d)", params->file_path, errno);
            close(fd);
            return BNCERT_RESULT_FILE_ERROR;
        }

        long file_size = file_stat.st_size;
        if (file_size <= 0 || file_size > BNCERT_MAX_DATA_SIZE) {
            ESP_LOGE(TAG, "Invalid certificate file size: %ld bytes (must be 1-%u, max 4KB)", 
                     file_size, BNCERT_MAX_DATA_SIZE);
            char error_msg[100];
            int len = snprintf(error_msg, sizeof(error_msg), 
                              "ERROR: Certificate file size %ld bytes exceeds 4KB limit\r\n", 
                              file_size);
            esp_at_port_write_data((uint8_t *)error_msg, len);
            close(fd);
            return BNCERT_RESULT_FILE_ERROR;
        }

        data_size = (size_t)file_size;

        // Allocate buffer
        data_buffer = malloc(data_size);
        if (!data_buffer) {
            ESP_LOGE(TAG, "Failed to allocate %u bytes for certificate data", 
                     (unsigned int)data_size);
            close(fd);
            return BNCERT_RESULT_MEMORY_ERROR;
        }

        // Read file data using POSIX read
        ssize_t bytes_read = read(fd, data_buffer, data_size);
        close(fd);

        if (bytes_read != (ssize_t)data_size) {
            ESP_LOGE(TAG, "Failed to read complete certificate file: %d/%u bytes (errno: %d)", 
                     (int)bytes_read, (unsigned int)data_size, errno);
            free(data_buffer);
            return BNCERT_RESULT_FILE_ERROR;
        }

        ESP_LOGI(TAG, "Successfully read %u bytes from certificate file", 
                 (unsigned int)data_size);

        // Validate certificate format before flashing
        if (!bncert_manager_validate_cert(data_buffer, data_size)) {
            ESP_LOGE(TAG, "Certificate file validation failed: %s", params->file_path);
            free(data_buffer);
            return BNCERT_RESULT_FILE_ERROR;
        }

    } else {
        // Use UART data
        data_buffer = params->uart_data;
        data_size = params->collected_size;

        if (!data_buffer || data_size == 0) {
            ESP_LOGE(TAG, "No UART data available for flashing");
            return BNCERT_RESULT_UART_ERROR;
        }

        // Validate certificate format before flashing
        if (!bncert_manager_validate_cert(data_buffer, data_size)) {
            ESP_LOGE(TAG, "UART certificate data validation failed");
            return BNCERT_RESULT_UART_ERROR;
        }

        ESP_LOGI(TAG, "Using %u bytes of UART data for flashing", 
                 (unsigned int)data_size);
    }

    // Validate flash address again with actual data size
    if (!bncert_validate_flash_address(params->flash_address, data_size)) {
        result = BNCERT_RESULT_INVALID_PARAMS;
        goto cleanup;
    }

    // Erase flash sector(s) first
    size_t sector_size = 4096; // Typical flash sector size
    size_t erase_size = ((data_size + sector_size - 1) / sector_size) * sector_size;
    
    // Calculate offset within partition
    uint32_t partition_offset = params->flash_address - s_cert_partition->address;
    
    ESP_LOGI(TAG, "Erasing %u bytes at partition offset 0x%08X (absolute: 0x%08X)", 
             (unsigned int)erase_size, (unsigned int)partition_offset, (unsigned int)params->flash_address);

    esp_err_t err = esp_partition_erase_range(s_cert_partition, partition_offset, erase_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase partition range: %s", esp_err_to_name(err));
        result = BNCERT_RESULT_FLASH_ERROR;
        goto cleanup;
    }

    // Write certificate data to partition
    ESP_LOGI(TAG, "Writing %u bytes to partition offset 0x%08X (absolute: 0x%08X)", 
             (unsigned int)data_size, (unsigned int)partition_offset, (unsigned int)params->flash_address);

    err = esp_partition_write(s_cert_partition, partition_offset, data_buffer, data_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write certificate to partition: %s", esp_err_to_name(err));
        result = BNCERT_RESULT_FLASH_ERROR;
        goto cleanup;
    }

    // Verify the written data
    uint8_t *verify_buffer = malloc(data_size);
    if (verify_buffer) {
        err = esp_partition_read(s_cert_partition, partition_offset, verify_buffer, data_size);
        if (err == ESP_OK) {
            if (memcmp(data_buffer, verify_buffer, data_size) == 0) {
                ESP_LOGI(TAG, "Certificate partition verification successful");
            } else {
                ESP_LOGE(TAG, "Certificate partition verification failed - data mismatch");
                result = BNCERT_RESULT_FLASH_ERROR;
            }
        } else {
            ESP_LOGW(TAG, "Certificate partition verification read failed: %s", esp_err_to_name(err));
        }
        free(verify_buffer);
    }

    if (result == BNCERT_RESULT_OK) {
        ESP_LOGI(TAG, "Certificate successfully flashed to 0x%08X (%u bytes)", 
                 (unsigned int)params->flash_address, (unsigned int)data_size);
        
        // Automatically register the certificate with the manager
        if (bncert_manager_register(params->flash_address, data_size)) {
            ESP_LOGI(TAG, "Certificate automatically registered with manager");
        } else {
            ESP_LOGW(TAG, "Failed to register certificate with manager (flash was successful)");
        }
        
        // Reload all certificates to ensure registry is up to date
        bncert_manager_reload_certificates();
    }

cleanup:
    // Free file data buffer (UART buffer is freed by cleanup_params)
    if (params->source_type == BNCERT_SOURCE_FILE && data_buffer) {
        free(data_buffer);
    }

    return result;
}

void bncert_cleanup_params(bncert_params_t *params)
{
    if (!params) {
        return;
    }

    if (params->uart_data) {
        free(params->uart_data);
        params->uart_data = NULL;
    }

    params->collected_size = 0;
}

const char* bncert_get_result_string(bncert_result_t result)
{
    switch (result) {
        case BNCERT_RESULT_OK:
            return "OK";
        case BNCERT_RESULT_INVALID_PARAMS:
            return "Invalid parameters";
        case BNCERT_RESULT_FILE_ERROR:
            return "File operation error";
        case BNCERT_RESULT_FLASH_ERROR:
            return "Flash operation error";
        case BNCERT_RESULT_MEMORY_ERROR:
            return "Memory allocation error";
        case BNCERT_RESULT_UART_ERROR:
            return "UART data collection error";
        default:
            return "Unknown error";
    }
}

void bncert_list_valid_addresses(void)
{
    if (!s_cert_partition) {
        esp_at_port_write_data((uint8_t *)"ERROR: Certificate partition not initialized\r\n", 47);
        return;
    }

    uint32_t partition_start = s_cert_partition->address;
    uint32_t partition_end = s_cert_partition->address + s_cert_partition->size;
    uint32_t partition_size = s_cert_partition->size;
    
    char response_buffer[256];
    
    // Send partition information
    int len = snprintf(response_buffer, sizeof(response_buffer),
                      "+BNCERT_ADDR:PARTITION,0x%08X,0x%08X,%u\r\n",
                      (unsigned int)partition_start,
                      (unsigned int)(partition_end - 1),
                      (unsigned int)partition_size);
    esp_at_port_write_data((uint8_t *)response_buffer, len);
    
    // Send capacity information
    uint32_t total_slots = (partition_end - partition_start) / 0x1000;
    len = snprintf(response_buffer, sizeof(response_buffer),
                  "+BNCERT_ADDR:CAPACITY,%u,4096,%u\r\n",
                  (unsigned int)total_slots,
                  (unsigned int)(partition_size / 1024));
    esp_at_port_write_data((uint8_t *)response_buffer, len);
    
    // Send valid addresses in groups of 4
    esp_at_port_write_data((uint8_t *)"+BNCERT_ADDR:ADDRESSES\r\n", 24);
    
    int count = 0;
    for (uint32_t addr = partition_start; addr < partition_end; addr += 0x1000) {
        if (count % 4 == 0) {
            if (count > 0) {
                esp_at_port_write_data((uint8_t *)"\r\n", 2);
            }
            len = snprintf(response_buffer, sizeof(response_buffer),
                          "+BNCERT_ADDR:0x%08X", (unsigned int)addr);
            esp_at_port_write_data((uint8_t *)response_buffer, len);
        } else {
            len = snprintf(response_buffer, sizeof(response_buffer),
                          ",0x%08X", (unsigned int)addr);
            esp_at_port_write_data((uint8_t *)response_buffer, len);
        }
        count++;
        
        // Limit output to avoid flooding
        if (count >= 16) {
            len = snprintf(response_buffer, sizeof(response_buffer),
                          "\r\n+BNCERT_ADDR:TOTAL,%u\r\n", (unsigned int)total_slots);
            esp_at_port_write_data((uint8_t *)response_buffer, len);
            break;
        }
    }
    
    if (count < 16 && count % 4 != 0) {
        esp_at_port_write_data((uint8_t *)"\r\n", 2);
    }
    
    // Send usage information
    esp_at_port_write_data((uint8_t *)"+BNCERT_ADDR:USAGE,\"AT+BNFLASH_CERT=<address>,<@file_or_bytes>\"\r\n", 64);
}
