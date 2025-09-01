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
        printf("ERROR: Empty file path for certificate\n");
        return false;
    }
    
    if (file_path[0] != '@') {
        ESP_LOGE(TAG, "Invalid certificate file path: %s (must start with @)", file_path);
        printf("ERROR: Certificate file path must start with @ (SD card prefix): %s\n", file_path);
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

    // Print valid address information
    printf("BNCERT: Certificate partition found at 0x%08X (%u bytes)\n", 
           (unsigned int)s_cert_partition->address, (unsigned int)s_cert_partition->size);
    
    bncert_list_valid_addresses();

    // Initialize certificate manager for automatic discovery
    if (!bncert_manager_init()) {
        ESP_LOGW(TAG, "Certificate manager initialization failed, but basic flashing will still work");
    }

    s_bncert_initialized = true;
    ESP_LOGI(TAG, "Certificate flashing subsystem initialized");
    printf("BNCERT: Certificate flashing subsystem initialized successfully\n");
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
        printf("ERROR: AT+BNFLASH_CERT requires exactly 2 parameters: <flash_address>,<data_source>\n");
        return ESP_AT_RESULT_CODE_ERROR;
    }

    // Parse flash address (parameter 0)
    int32_t addr_value;
    if (esp_at_get_para_as_digit(0, &addr_value) != ESP_AT_PARA_PARSE_RESULT_OK) {
        printf("ERROR: Invalid flash address parameter\n");
        return ESP_AT_RESULT_CODE_ERROR;
    }
    params->flash_address = (uint32_t)addr_value;

    // Parse data source (parameter 1)
    uint8_t *data_source_str = NULL;
    if (esp_at_get_para_as_str(1, &data_source_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
        printf("ERROR: Invalid data source parameter\n");
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (data_source_str[0] == '@') {
        // File source - validate the @ prefix
        if (!validate_cert_file_path_prefix((char *)data_source_str)) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        params->source_type = BNCERT_SOURCE_FILE;
        
        if (strlen((char *)data_source_str) > BNCERT_MAX_FILE_PATH_LENGTH) {
            printf("ERROR: File path too long (max %d characters)\n", BNCERT_MAX_FILE_PATH_LENGTH);
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        strncpy(params->file_path, (char *)data_source_str, BNCERT_MAX_FILE_PATH_LENGTH);
        params->file_path[BNCERT_MAX_FILE_PATH_LENGTH] = '\0';
        
        // Normalize the file path (remove @ and prepend mount point)
        normalize_path_with_mount_point(params->file_path, BNCERT_MAX_FILE_PATH_LENGTH);
        
        ESP_LOGI(TAG, "Certificate source: file %s", params->file_path);
    } else {
        // Check if it's a valid numeric value for UART data source
        char *endptr;
        long data_size = strtol((char *)data_source_str, &endptr, 10);
        
        if (*endptr != '\0' || data_size < 0 || data_size > BNCERT_MAX_DATA_SIZE) {
            // Not a valid number and doesn't start with @ - invalid
            printf("ERROR: Invalid data source: %s (must be numeric 0-%u or file path starting with @)\n", 
                   (char *)data_source_str, BNCERT_MAX_DATA_SIZE);
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        // Valid UART data source
        params->source_type = BNCERT_SOURCE_UART;
        params->data_size = (size_t)data_size;
        ESP_LOGI(TAG, "Certificate source: UART (%u bytes)", (unsigned int)params->data_size);
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
        printf("ERROR: Address must be 4KB aligned\n");
        return false;
    }

    // Check if address is within certificate partition bounds
    if (address < partition_start || address >= partition_end) {
        ESP_LOGE(TAG, "Address 0x%08X outside certificate partition bounds", (unsigned int)address);
        printf("ERROR: Address outside certificate partition\n");
        return false;
    }

    // Check that address + size doesn't exceed partition end
    if (address + size > partition_end) {
        ESP_LOGE(TAG, "Certificate data would exceed partition boundary");
        printf("ERROR: Certificate data exceeds partition boundary\n");
        return false;
    }

    // Check size is reasonable
    if (size == 0 || size > BNCERT_MAX_DATA_SIZE) {
        ESP_LOGE(TAG, "Invalid certificate size: %u", (unsigned int)size);
        printf("ERROR: Invalid certificate size\n");
        return false;
    }

    ESP_LOGI(TAG, "Address 0x%08X validated for %u bytes", (unsigned int)address, (unsigned int)size);
    return true;
}

bool bncert_collect_uart_data(bncert_params_t *params)
{
    if (!params || params->source_type != BNCERT_SOURCE_UART) {
        ESP_LOGE(TAG, "Invalid parameters for UART data collection");
        return false;
    }

    if (params->data_size == 0) {
        ESP_LOGW(TAG, "Zero bytes requested - nothing to collect");
        params->collected_size = 0;
        return true;
    }

    // Allocate buffer for UART data
    params->uart_data = malloc(params->data_size);
    if (!params->uart_data) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes for UART data", 
                 (unsigned int)params->data_size);
        return false;
    }

    ESP_LOGI(TAG, "Collecting %u bytes from UART", (unsigned int)params->data_size);

    // Send prompt to indicate ready for data
    const char *prompt = ">";
    esp_at_port_write_data((uint8_t *)prompt, strlen(prompt));

    // Collect data from UART
    size_t bytes_received = 0;
    uint32_t timeout_ms = 30000; // 30 second timeout
    
    while (bytes_received < params->data_size) {
        uint8_t byte;
        int32_t len = esp_at_port_read_data(&byte, 1);
        
        if (len > 0) {
            params->uart_data[bytes_received] = byte;
            bytes_received++;
        } else {
            // Yield to other tasks and continue waiting
            vTaskDelay(pdMS_TO_TICKS(10));
            
            // In a real implementation, you would implement proper timeout handling here
            // For now, we rely on the AT command timeout mechanism
        }
    }

    params->collected_size = bytes_received;
    ESP_LOGI(TAG, "Successfully collected %u bytes from UART", 
             (unsigned int)params->collected_size);

    return true;
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
            ESP_LOGE(TAG, "Invalid certificate file size: %ld bytes", file_size);
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
        printf("BNCERT: Successfully loaded certificate from file: %s (%u bytes)\n", 
               params->file_path, (unsigned int)data_size);

    } else {
        // Use UART data
        data_buffer = params->uart_data;
        data_size = params->collected_size;

        if (!data_buffer || data_size == 0) {
            ESP_LOGE(TAG, "No UART data available for flashing");
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
        printf("BNCERT: Certificate successfully saved to flash at 0x%08X (%u bytes)\n", 
               (unsigned int)params->flash_address, (unsigned int)data_size);
        
        // Automatically register the certificate with the manager
        if (bncert_manager_register(params->flash_address, data_size)) {
            ESP_LOGI(TAG, "Certificate automatically registered with manager");
            printf("BNCERT: Certificate automatically registered for TLS use\n");
        } else {
            ESP_LOGW(TAG, "Failed to register certificate with manager (flash was successful)");
            printf("BNCERT: Warning - Certificate saved but failed to register for automatic TLS use\n");
        }
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
        printf("BNCERT: Certificate partition not initialized\n");
        return;
    }

    uint32_t partition_start = s_cert_partition->address;
    uint32_t partition_end = s_cert_partition->address + s_cert_partition->size;
    uint32_t partition_size = s_cert_partition->size;
    
    printf("BNCERT: Valid certificate storage addresses (4KB aligned):\n");
    printf("BNCERT: Partition range: 0x%08X - 0x%08X (%u bytes)\n", 
           (unsigned int)partition_start, (unsigned int)(partition_end - 1), (unsigned int)partition_size);
    printf("BNCERT: Available 4KB-aligned addresses:\n");
    
    // List all valid 4KB aligned addresses
    int count = 0;
    for (uint32_t addr = partition_start; addr < partition_end; addr += 0x1000) {
        if (count % 4 == 0) {
            printf("BNCERT:   ");
        }
        printf("0x%08X", (unsigned int)addr);
        count++;
        
        if (count % 4 == 0) {
            printf("\n");
        } else {
            printf("  ");
        }
        
        // Stop at reasonable number to avoid console spam
        if (count >= 64) {
            printf("\nBNCERT:   ... (total %u valid addresses)\n", 
                   (unsigned int)((partition_end - partition_start) / 0x1000));
            break;
        }
    }
    
    if (count % 4 != 0) {
        printf("\n");
    }
    
    printf("BNCERT: Total capacity: %u slots (4KB each = %u KB total)\n", 
           (unsigned int)((partition_end - partition_start) / 0x1000),
           (unsigned int)(partition_size / 1024));
    printf("BNCERT: Address alignment: All addresses must be 4KB (0x1000) aligned\n");
    printf("BNCERT: Usage: AT+BNFLASH_CERT=<address>,<@file_path_or_byte_count>\n");
}
