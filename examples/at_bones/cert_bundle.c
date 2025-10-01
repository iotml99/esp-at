/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cert_bundle.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_crc.h"
#include "esp_at.h"
#include "esp_vfs_fat.h"
#include "driver/uart.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "CERT_BUNDLE";

/**
 * @brief Streaming PEM validator state
 */
typedef struct {
    enum {
        PEM_STATE_LOOKING_FOR_BEGIN,
        PEM_STATE_IN_CERTIFICATE,
        PEM_STATE_LOOKING_FOR_END
    } state;
    char marker_buffer[32];     // Buffer for partial markers across chunks
    size_t marker_pos;          // Position in marker buffer
    int cert_count;             // Number of complete certificates found
    bool has_error;             // Validation error flag
} pem_validator_t;

// Forward declarations for PEM validator functions
static void pem_validator_init(pem_validator_t *validator);
static void pem_validator_process_chunk(pem_validator_t *validator, const uint8_t *chunk, size_t chunk_size);
static bool pem_validator_finalize(pem_validator_t *validator);

// Forward declaration for certificate preloading
static bool preload_certificate_bundle(void);

// System state
static const esp_partition_t *s_cert_partition = NULL;
static bool s_bundle_initialized = false;
static const char *s_hardcoded_bundle = NULL;
static size_t s_hardcoded_size = 0;
static cert_bundle_flash_context_t s_flash_context = {0};

// Cached validation state (to avoid repeated validation)
static struct {
    bool validation_done;           // Whether validation has been performed
    cert_bundle_status_t status;    // Cached validation result
    uint32_t bundle_size;           // Cached bundle size
    uint32_t bundle_crc32;          // Cached bundle CRC
    const char *active_bundle_ptr;  // Pointer to active bundle (flash or hardcoded)
    size_t active_bundle_size;      // Size of active bundle
    char *flash_bundle_buffer;      // RAM buffer for flash bundle (allocated when needed)
} s_bundle_cache = {0};

/**
 * @brief Calculate CRC32 of data
 */
static uint32_t calc_crc32(const void *data, size_t size)
{
    return esp_crc32_le(0, (const uint8_t *)data, size);
}

/**
 * @brief Calculate CRC32 of data in flash partition (chunked read)
 */
static uint32_t calc_flash_crc32(uint32_t offset, size_t size)
{
    const size_t chunk_size = 1024;
    uint8_t *chunk_buffer = malloc(chunk_size);
    if (!chunk_buffer) {
        ESP_LOGE(TAG, "Failed to allocate CRC calculation buffer");
        return 0;
    }

    uint32_t crc = 0;
    size_t remaining = size;
    uint32_t current_offset = offset;

    while (remaining > 0) {
        size_t to_read = (remaining < chunk_size) ? remaining : chunk_size;
        
        esp_err_t err = esp_partition_read(s_cert_partition, current_offset, chunk_buffer, to_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read flash for CRC: %s", esp_err_to_name(err));
            free(chunk_buffer);
            return 0;
        }

        crc = esp_crc32_le(crc, chunk_buffer, to_read);
        remaining -= to_read;
        current_offset += to_read;
    }

    free(chunk_buffer);
    return crc;
}

/**
 * @brief Validate bundle in flash partition (with caching)
 */
static cert_bundle_status_t validate_flash_bundle(uint32_t *bundle_size, uint32_t *bundle_crc)
{
    if (!s_cert_partition) {
        return CERT_BUNDLE_STATUS_NONE;
    }

    // Return cached result if validation already done
    if (s_bundle_cache.validation_done) {
        if (bundle_size) *bundle_size = s_bundle_cache.bundle_size;
        if (bundle_crc) *bundle_crc = s_bundle_cache.bundle_crc32;
        ESP_LOGD(TAG, "Using cached validation result: status=%d", s_bundle_cache.status);
        return s_bundle_cache.status;
    }

    ESP_LOGI(TAG, "Performing flash bundle validation...");

    // Read header
    cert_bundle_header_t header;
    esp_err_t err = esp_partition_read(s_cert_partition, 0, &header, sizeof(header));
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "Cannot read bundle header: %s", esp_err_to_name(err));
        s_bundle_cache.status = CERT_BUNDLE_STATUS_NONE;
        s_bundle_cache.validation_done = true;
        return CERT_BUNDLE_STATUS_NONE;
    }

    // Check if bundle exists
    if (header.bundle_length == 0 || header.bundle_length == 0xFFFFFFFF) {
        ESP_LOGD(TAG, "No bundle stored (length: %u)", (unsigned int)header.bundle_length);
        s_bundle_cache.status = CERT_BUNDLE_STATUS_NONE;
        s_bundle_cache.validation_done = true;
        return CERT_BUNDLE_STATUS_NONE;
    }

    // Check size validity
    if (header.bundle_length > CERT_BUNDLE_MAX_SIZE) {
        ESP_LOGW(TAG, "Bundle size invalid: %u bytes (max %u)", 
                (unsigned int)header.bundle_length, CERT_BUNDLE_MAX_SIZE);
        s_bundle_cache.status = CERT_BUNDLE_STATUS_CORRUPTED;
        s_bundle_cache.bundle_size = header.bundle_length;
        s_bundle_cache.bundle_crc32 = header.bundle_crc32;
        s_bundle_cache.validation_done = true;
        return CERT_BUNDLE_STATUS_CORRUPTED;
    }

    // Calculate actual CRC (this is expensive - only do once)
    uint32_t calc_crc = calc_flash_crc32(CERT_BUNDLE_HEADER_SIZE, header.bundle_length);
    if (calc_crc == 0) {
        ESP_LOGE(TAG, "Failed to calculate bundle CRC");
        s_bundle_cache.status = CERT_BUNDLE_STATUS_CORRUPTED;
        s_bundle_cache.bundle_size = header.bundle_length;
        s_bundle_cache.bundle_crc32 = header.bundle_crc32;
        s_bundle_cache.validation_done = true;
        return CERT_BUNDLE_STATUS_CORRUPTED;
    }

    // Validate CRC
    if (header.bundle_crc32 != calc_crc) {
        ESP_LOGW(TAG, "Bundle CRC mismatch: stored=0x%08X, calculated=0x%08X", 
                (unsigned int)header.bundle_crc32, (unsigned int)calc_crc);
        s_bundle_cache.status = CERT_BUNDLE_STATUS_CORRUPTED;
        s_bundle_cache.bundle_size = header.bundle_length;
        s_bundle_cache.bundle_crc32 = header.bundle_crc32;
        s_bundle_cache.validation_done = true;
        return CERT_BUNDLE_STATUS_CORRUPTED;
    }

    // Cache valid results (NOTE: Don't cache flash pointer - will read to RAM when needed)
    s_bundle_cache.status = CERT_BUNDLE_STATUS_VALID;
    s_bundle_cache.bundle_size = header.bundle_length;
    s_bundle_cache.bundle_crc32 = header.bundle_crc32;
    s_bundle_cache.active_bundle_ptr = NULL; // Will be allocated when needed
    s_bundle_cache.active_bundle_size = header.bundle_length;
    s_bundle_cache.validation_done = true;

    // Return bundle info
    if (bundle_size) *bundle_size = header.bundle_length;
    if (bundle_crc) *bundle_crc = header.bundle_crc32;

    ESP_LOGI(TAG, "Valid bundle found and cached: %u bytes, CRC=0x%08X", 
            (unsigned int)header.bundle_length, (unsigned int)header.bundle_crc32);
    return CERT_BUNDLE_STATUS_VALID;
}

/**
 * @brief Preload certificate bundle from flash into memory
 * 
 * This function loads the validated certificate bundle from flash into RAM
 * at system startup or when certificates change, ensuring immediate availability
 * for SSL operations without flash access delays.
 * 
 * @return true on success, false on failure
 */
static bool preload_certificate_bundle(void)
{
    if (s_bundle_cache.status != CERT_BUNDLE_STATUS_VALID || s_bundle_cache.active_bundle_size == 0) {
        ESP_LOGD(TAG, "No valid bundle to preload");
        return false;
    }

    // Free existing buffer if already allocated
    if (s_bundle_cache.flash_bundle_buffer) {
        free(s_bundle_cache.flash_bundle_buffer);
        s_bundle_cache.flash_bundle_buffer = NULL;
    }

    ESP_LOGI(TAG, "Preloading certificate bundle into memory (%u bytes)", 
             (unsigned int)s_bundle_cache.active_bundle_size);

    // Allocate buffer for certificate bundle
    s_bundle_cache.flash_bundle_buffer = malloc(s_bundle_cache.active_bundle_size);
    if (!s_bundle_cache.flash_bundle_buffer) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes for certificate bundle preload", 
                 (unsigned int)s_bundle_cache.active_bundle_size);
        return false;
    }

    // Read certificate bundle from flash into RAM
    esp_err_t err = esp_partition_read(s_cert_partition, CERT_BUNDLE_HEADER_SIZE, 
                                     s_bundle_cache.flash_bundle_buffer, 
                                     s_bundle_cache.active_bundle_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to preload certificate bundle from flash: %s", esp_err_to_name(err));
        free(s_bundle_cache.flash_bundle_buffer);
        s_bundle_cache.flash_bundle_buffer = NULL;
        return false;
    }

    // Log memory usage for monitoring
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    ESP_LOGI(TAG, "Certificate bundle preloaded successfully - ready for SSL operations");
    ESP_LOGI(TAG, "Memory after cert preload: free=%u bytes, min_free=%u bytes", 
             (unsigned int)free_heap, (unsigned int)min_free_heap);
    return true;
}

/**
 * @brief Invalidate cached validation (call when bundle changes)
 */
static void invalidate_bundle_cache(void)
{
    ESP_LOGI(TAG, "Invalidating bundle cache");
    
    // Free any allocated flash bundle buffer
    if (s_bundle_cache.flash_bundle_buffer) {
        free(s_bundle_cache.flash_bundle_buffer);
        s_bundle_cache.flash_bundle_buffer = NULL;
    }
    
    memset(&s_bundle_cache, 0, sizeof(s_bundle_cache));
}

bool cert_bundle_init(const char *hardcoded_bundle, size_t hardcoded_size)
{
    if (s_bundle_initialized) {
        ESP_LOGD(TAG, "Certificate bundle system already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing certificate bundle system");

    // Store hardcoded bundle reference
    s_hardcoded_bundle = hardcoded_bundle;
    s_hardcoded_size = hardcoded_size;

    // Find certificate partition
    s_cert_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, CERT_BUNDLE_PARTITION_SUBTYPE, NULL);
    if (!s_cert_partition) {
        ESP_LOGE(TAG, "Certificate partition (subtype 0x%02X) not found", CERT_BUNDLE_PARTITION_SUBTYPE);
        return false;
    }

    ESP_LOGI(TAG, "Found certificate partition: 0x%08X (%u KB)", 
             (unsigned int)s_cert_partition->address, 
             (unsigned int)(s_cert_partition->size / 1024));

    // Validate partition size
    if (s_cert_partition->size < CERT_BUNDLE_HEADER_SIZE) {
        ESP_LOGE(TAG, "Certificate partition too small: %u bytes", (unsigned int)s_cert_partition->size);
        return false;
    }

    // Initialize flash context semaphore
    s_flash_context.uart_semaphore = xSemaphoreCreateMutex();
    if (!s_flash_context.uart_semaphore) {
        ESP_LOGE(TAG, "Failed to create UART semaphore");
        return false;
    }

    s_bundle_initialized = true;

    // Validate and preload certificate bundle into memory
    cert_bundle_status_t status = validate_flash_bundle(NULL, NULL);
    switch (status) {
        case CERT_BUNDLE_STATUS_VALID:
            ESP_LOGI(TAG, "Valid certificate bundle found in flash");
            // Preload certificate bundle into memory
            if (!preload_certificate_bundle()) {
                ESP_LOGW(TAG, "Failed to preload certificate bundle - will use hardcoded fallback");
            }
            break;
        case CERT_BUNDLE_STATUS_CORRUPTED:
            ESP_LOGW(TAG, "Corrupted certificate bundle found - will use hardcoded fallback");
            break;
        case CERT_BUNDLE_STATUS_NONE:
            ESP_LOGI(TAG, "No certificate bundle in flash - will use hardcoded fallback");
            break;
    }

    ESP_LOGI(TAG, "Certificate bundle system initialized successfully");
    return true;
}

void cert_bundle_deinit(void)
{
    if (!s_bundle_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing certificate bundle system");

    // Clean up semaphore
    if (s_flash_context.uart_semaphore) {
        vSemaphoreDelete(s_flash_context.uart_semaphore);
        s_flash_context.uart_semaphore = NULL;
    }

    // Clean up cached bundle buffer
    invalidate_bundle_cache();

    // Reset state
    s_cert_partition = NULL;
    s_hardcoded_bundle = NULL;
    s_hardcoded_size = 0;
    s_bundle_initialized = false;
    memset(&s_flash_context, 0, sizeof(s_flash_context));
}

cert_bundle_result_t cert_bundle_get(const char **bundle_ptr, size_t *bundle_size)
{
    if (!bundle_ptr || !bundle_size) {
        return CERT_BUNDLE_ERROR_INVALID_PARAM;
    }

    if (!s_bundle_initialized) {
        return CERT_BUNDLE_ERROR_PARTITION;
    }

    // Check if we have a preloaded certificate bundle in memory
    if (s_bundle_cache.validation_done && 
        s_bundle_cache.status == CERT_BUNDLE_STATUS_VALID && 
        s_bundle_cache.flash_bundle_buffer != NULL) {
        ESP_LOGD(TAG, "Using preloaded certificate bundle from memory");
        *bundle_ptr = s_bundle_cache.flash_bundle_buffer;
        *bundle_size = s_bundle_cache.active_bundle_size;
        return CERT_BUNDLE_OK;
    }

    // If no preloaded bundle, perform validation and try to load
    if (!s_bundle_cache.validation_done) {
        uint32_t flash_bundle_size = 0;
        cert_bundle_status_t status = validate_flash_bundle(&flash_bundle_size, NULL);
        
        if (status == CERT_BUNDLE_STATUS_VALID) {
            // Try to preload the bundle
            if (preload_certificate_bundle()) {
                ESP_LOGI(TAG, "Certificate bundle loaded on-demand");
                *bundle_ptr = s_bundle_cache.flash_bundle_buffer;
                *bundle_size = s_bundle_cache.active_bundle_size;
                return CERT_BUNDLE_OK;
            } else {
                ESP_LOGW(TAG, "Failed to load certificate bundle on-demand");
            }
        }
    }

    // Fallback to hardcoded bundle
    if (s_hardcoded_bundle && s_hardcoded_size > 0) {
        ESP_LOGD(TAG, "Using hardcoded certificate bundle");
        *bundle_ptr = s_hardcoded_bundle;
        *bundle_size = s_hardcoded_size;
        return CERT_BUNDLE_OK;
    }

    ESP_LOGE(TAG, "No certificate bundle available (flash invalid, no hardcoded)");
    return CERT_BUNDLE_ERROR_PARTITION;
}

cert_bundle_result_t cert_bundle_get_info(cert_bundle_info_t *info)
{
    if (!info) {
        return CERT_BUNDLE_ERROR_INVALID_PARAM;
    }

    if (!s_bundle_initialized) {
        return CERT_BUNDLE_ERROR_PARTITION;
    }

    // Validate flash bundle
    uint32_t bundle_size = 0;
    uint32_t bundle_crc = 0;
    cert_bundle_status_t status = validate_flash_bundle(&bundle_size, &bundle_crc);

    info->status = status;
    info->bundle_size = bundle_size;
    info->bundle_crc32 = bundle_crc;

    return CERT_BUNDLE_OK;
}

cert_bundle_result_t cert_bundle_flash_from_sd(const char *file_path)
{
    if (!file_path) {
        return CERT_BUNDLE_ERROR_INVALID_PARAM;
    }

    if (!s_bundle_initialized || !s_cert_partition) {
        return CERT_BUNDLE_ERROR_PARTITION;
    }

    ESP_LOGI(TAG, "Flashing certificate bundle from SD: %s", file_path);

    // Open file
    FILE *file = fopen(file_path, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file: %s", file_path);
        return CERT_BUNDLE_ERROR_INVALID_PARAM;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0 || file_size > CERT_BUNDLE_MAX_SIZE) {
        ESP_LOGE(TAG, "Invalid file size: %ld bytes (max %u)", file_size, CERT_BUNDLE_MAX_SIZE);
        fclose(file);
        return CERT_BUNDLE_ERROR_TOO_LARGE;
    }

    ESP_LOGI(TAG, "Processing file: %ld bytes", file_size);

    // Use small buffer for chunked operations
    const size_t chunk_size = 1024;
    uint8_t *chunk_buffer = malloc(chunk_size);
    if (!chunk_buffer) {
        ESP_LOGE(TAG, "Failed to allocate %u byte chunk buffer", (unsigned int)chunk_size);
        fclose(file);
        return CERT_BUNDLE_ERROR_MEMORY;
    }

    // First pass: Calculate CRC and validate PEM format
    uint32_t bundle_crc = 0;
    size_t total_processed = 0;
    pem_validator_t pem_validator;
    pem_validator_init(&pem_validator);

    ESP_LOGI(TAG, "Pass 1: Calculating CRC and validating format...");
    while (total_processed < file_size) {
        size_t to_read = (file_size - total_processed < chunk_size) ? 
                         (file_size - total_processed) : chunk_size;
        
        size_t bytes_read = fread(chunk_buffer, 1, to_read, file);
        if (bytes_read != to_read) {
            ESP_LOGE(TAG, "Failed to read chunk: %u/%u bytes", (unsigned int)bytes_read, (unsigned int)to_read);
            free(chunk_buffer);
            fclose(file);
            return CERT_BUNDLE_ERROR_INVALID_PARAM;
        }

        // Update CRC
        bundle_crc = esp_crc32_le(bundle_crc, chunk_buffer, bytes_read);

        // Process chunk for PEM validation
        pem_validator_process_chunk(&pem_validator, chunk_buffer, bytes_read);
        
        // Check for validation errors
        if (pem_validator.has_error) {
            ESP_LOGE(TAG, "PEM validation error in chunk at offset %u", (unsigned int)total_processed);
            free(chunk_buffer);
            fclose(file);
            return CERT_BUNDLE_ERROR_INVALID_PARAM;
        }

        total_processed += bytes_read;
        
        // Progress indication
        if (total_processed % (32 * 1024) == 0 || total_processed == file_size) {
            ESP_LOGD(TAG, "Processed %u/%ld bytes", (unsigned int)total_processed, file_size);
        }
    }

    // Finalize PEM validation
    if (!pem_validator_finalize(&pem_validator)) {
        ESP_LOGE(TAG, "PEM validation failed");
        free(chunk_buffer);
        fclose(file);
        return CERT_BUNDLE_ERROR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "PEM validation passed, CRC32=0x%08X", (unsigned int)bundle_crc);

    // Prepare header
    cert_bundle_header_t header;
    header.bundle_length = (uint32_t)file_size;
    header.bundle_crc32 = bundle_crc;

    // Erase partition
    ESP_LOGI(TAG, "Erasing certificate partition...");
    esp_err_t err = esp_partition_erase_range(s_cert_partition, 0, s_cert_partition->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase partition: %s", esp_err_to_name(err));
        free(chunk_buffer);
        fclose(file);
        return CERT_BUNDLE_ERROR_WRITE;
    }

    // Write header
    ESP_LOGI(TAG, "Writing bundle header...");
    err = esp_partition_write(s_cert_partition, 0, &header, sizeof(header));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write header: %s", esp_err_to_name(err));
        free(chunk_buffer);
        fclose(file);
        return CERT_BUNDLE_ERROR_WRITE;
    }

    // Second pass: Write data to flash in chunks
    fseek(file, 0, SEEK_SET); // Reset file position
    total_processed = 0;
    uint32_t flash_offset = CERT_BUNDLE_HEADER_SIZE;

    ESP_LOGI(TAG, "Pass 2: Writing bundle data (%u bytes)...", (unsigned int)file_size);
    while (total_processed < file_size) {
        size_t to_read = (file_size - total_processed < chunk_size) ? 
                         (file_size - total_processed) : chunk_size;
        
        size_t bytes_read = fread(chunk_buffer, 1, to_read, file);
        if (bytes_read != to_read) {
            ESP_LOGE(TAG, "Failed to read chunk for writing: %u/%u bytes", 
                    (unsigned int)bytes_read, (unsigned int)to_read);
            free(chunk_buffer);
            fclose(file);
            return CERT_BUNDLE_ERROR_INVALID_PARAM;
        }

        // Write chunk to flash
        err = esp_partition_write(s_cert_partition, flash_offset, chunk_buffer, bytes_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write chunk to flash: %s", esp_err_to_name(err));
            free(chunk_buffer);
            fclose(file);
            return CERT_BUNDLE_ERROR_WRITE;
        }

        flash_offset += bytes_read;
        total_processed += bytes_read;
        
        // Progress indication
        if (total_processed % (32 * 1024) == 0 || total_processed == file_size) {
            ESP_LOGD(TAG, "Written %u/%ld bytes", (unsigned int)total_processed, file_size);
        }
    }

    free(chunk_buffer);
    fclose(file);

    // Invalidate cache before validation since bundle has changed
    invalidate_bundle_cache();

    // Verify written data
    if (validate_flash_bundle(NULL, NULL) != CERT_BUNDLE_STATUS_VALID) {
        ESP_LOGE(TAG, "Bundle validation failed after write");
        return CERT_BUNDLE_ERROR_CRC;
    }

    // Preload the new certificate bundle into memory
    if (!preload_certificate_bundle()) {
        ESP_LOGW(TAG, "Certificate bundle flashed but failed to preload into memory");
    }

    ESP_LOGI(TAG, "Certificate bundle flashed successfully: %u bytes, CRC=0x%08X", 
            (unsigned int)file_size, (unsigned int)bundle_crc);

    return CERT_BUNDLE_OK;
}

cert_bundle_result_t cert_bundle_flash_from_uart(size_t bundle_size)
{
    if (bundle_size == 0 || bundle_size > CERT_BUNDLE_MAX_SIZE) {
        return CERT_BUNDLE_ERROR_INVALID_PARAM;
    }

    if (!s_bundle_initialized || !s_cert_partition) {
        return CERT_BUNDLE_ERROR_PARTITION;
    }

    // Take semaphore
    if (xSemaphoreTake(s_flash_context.uart_semaphore, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take UART semaphore");
        return CERT_BUNDLE_ERROR_SEMAPHORE;
    }

    ESP_LOGI(TAG, "Flashing certificate bundle from UART: %u bytes", (unsigned int)bundle_size);

    // Use small buffer for chunked operations
    const size_t chunk_size = 1024;
    uint8_t *chunk_buffer = malloc(chunk_size);
    if (!chunk_buffer) {
        ESP_LOGE(TAG, "Failed to allocate %u byte chunk buffer", (unsigned int)chunk_size);
        xSemaphoreGive(s_flash_context.uart_semaphore);
        return CERT_BUNDLE_ERROR_MEMORY;
    }

    // Send prompt and collect data
    esp_at_port_write_data((uint8_t *)">", 1);
    
    size_t total_received = 0;
    uint32_t bundle_crc = 0;
    pem_validator_t pem_validator;
    pem_validator_init(&pem_validator);
    const TickType_t timeout_per_chunk = pdMS_TO_TICKS(10000); // 10s timeout per chunk

    // Erase partition first (before collecting data)
    ESP_LOGI(TAG, "Erasing certificate partition...");
    esp_err_t err = esp_partition_erase_range(s_cert_partition, 0, s_cert_partition->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase partition: %s", esp_err_to_name(err));
        free(chunk_buffer);
        xSemaphoreGive(s_flash_context.uart_semaphore);
        return CERT_BUNDLE_ERROR_WRITE;
    }

    // Reserve space for header - we'll write it after collecting all data
    uint32_t flash_offset = CERT_BUNDLE_HEADER_SIZE;
    
    ESP_LOGI(TAG, "Collecting and writing data in chunks...");
    while (total_received < bundle_size) {
        size_t remaining = bundle_size - total_received;
        size_t to_receive = (remaining < chunk_size) ? remaining : chunk_size;
        
        // Receive chunk with timeout
        size_t received = 0;
        TickType_t start_time = xTaskGetTickCount();
        
        while (received < to_receive) {
            if ((xTaskGetTickCount() - start_time) > timeout_per_chunk) {
                ESP_LOGE(TAG, "UART timeout waiting for data");
                free(chunk_buffer);
                xSemaphoreGive(s_flash_context.uart_semaphore);
                return CERT_BUNDLE_ERROR_UART;
            }
            
            int32_t chunk_received = esp_at_port_read_data(chunk_buffer + received, 
                                                          to_receive - received);
            if (chunk_received > 0) {
                received += chunk_received;
            }
            
            if (chunk_received <= 0) {
                vTaskDelay(pdMS_TO_TICKS(10)); // Small delay if no data
            }
        }

        // Update CRC with received chunk
        bundle_crc = esp_crc32_le(bundle_crc, chunk_buffer, received);

        // Process chunk for PEM validation
        pem_validator_process_chunk(&pem_validator, chunk_buffer, received);
        
        // Check for validation errors
        if (pem_validator.has_error) {
            ESP_LOGE(TAG, "PEM validation error in UART chunk at offset %u", (unsigned int)total_received);
            free(chunk_buffer);
            xSemaphoreGive(s_flash_context.uart_semaphore);
            return CERT_BUNDLE_ERROR_INVALID_PARAM;
        }

        // Write chunk directly to flash
        err = esp_partition_write(s_cert_partition, flash_offset, chunk_buffer, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write chunk to flash: %s", esp_err_to_name(err));
            free(chunk_buffer);
            xSemaphoreGive(s_flash_context.uart_semaphore);
            return CERT_BUNDLE_ERROR_WRITE;
        }

        flash_offset += received;
        total_received += received;
        
        // Progress indication
        if (total_received % (16 * 1024) == 0 || total_received == bundle_size) {
            ESP_LOGD(TAG, "Received and written %u/%u bytes", (unsigned int)total_received, (unsigned int)bundle_size);
        }
    }

    free(chunk_buffer);

    // Finalize PEM validation
    if (!pem_validator_finalize(&pem_validator)) {
        ESP_LOGE(TAG, "PEM validation failed");
        xSemaphoreGive(s_flash_context.uart_semaphore);
        return CERT_BUNDLE_ERROR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Data collection complete. PEM validation passed (%d certificates), CRC32=0x%08X", 
             pem_validator.cert_count, (unsigned int)bundle_crc);

    // Prepare and write header
    cert_bundle_header_t header;
    header.bundle_length = (uint32_t)bundle_size;
    header.bundle_crc32 = bundle_crc;

    ESP_LOGI(TAG, "Writing bundle header...");
    err = esp_partition_write(s_cert_partition, 0, &header, sizeof(header));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write header: %s", esp_err_to_name(err));
        xSemaphoreGive(s_flash_context.uart_semaphore);
        return CERT_BUNDLE_ERROR_WRITE;
    }

    xSemaphoreGive(s_flash_context.uart_semaphore);

    // Invalidate cache before validation since bundle has changed
    invalidate_bundle_cache();

    // Verify written data
    if (validate_flash_bundle(NULL, NULL) != CERT_BUNDLE_STATUS_VALID) {
        ESP_LOGE(TAG, "Bundle validation failed after write");
        return CERT_BUNDLE_ERROR_CRC;
    }

    // Preload the new certificate bundle into memory
    if (!preload_certificate_bundle()) {
        ESP_LOGW(TAG, "Certificate bundle flashed but failed to preload into memory");
    }

    ESP_LOGI(TAG, "Certificate bundle flashed successfully: %u bytes, CRC=0x%08X", 
            (unsigned int)bundle_size, (unsigned int)bundle_crc);

    return CERT_BUNDLE_OK;
}

cert_bundle_result_t cert_bundle_clear(void)
{
    if (!s_bundle_initialized || !s_cert_partition) {
        return CERT_BUNDLE_ERROR_PARTITION;
    }

    ESP_LOGI(TAG, "Clearing certificate bundle partition");

    esp_err_t err = esp_partition_erase_range(s_cert_partition, 0, s_cert_partition->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase partition: %s", esp_err_to_name(err));
        return CERT_BUNDLE_ERROR_WRITE;
    }

    ESP_LOGI(TAG, "Certificate bundle cleared successfully");
    
    // Invalidate cache since bundle has been cleared
    invalidate_bundle_cache();
    
    return CERT_BUNDLE_OK;
}

/**
 * @brief Process a chunk of data for PEM validation
 */
static void pem_validator_process_chunk(pem_validator_t *validator, const uint8_t *chunk, size_t chunk_size)
{
    const char *cert_begin = "-----BEGIN CERTIFICATE-----";
    const char *cert_end = "-----END CERTIFICATE-----";
    const size_t begin_len = strlen(cert_begin);
    const size_t end_len = strlen(cert_end);
    
    for (size_t i = 0; i < chunk_size && !validator->has_error; i++) {
        char c = (char)chunk[i];
        
        // Add character to marker buffer
        if (validator->marker_pos < sizeof(validator->marker_buffer) - 1) {
            validator->marker_buffer[validator->marker_pos++] = c;
            validator->marker_buffer[validator->marker_pos] = '\0';
        } else {
            // Shift buffer left and add new character
            memmove(validator->marker_buffer, validator->marker_buffer + 1, sizeof(validator->marker_buffer) - 2);
            validator->marker_buffer[sizeof(validator->marker_buffer) - 2] = c;
            validator->marker_buffer[sizeof(validator->marker_buffer) - 1] = '\0';
            validator->marker_pos = sizeof(validator->marker_buffer) - 1;
        }
        
        // Check for state transitions
        switch (validator->state) {
            case PEM_STATE_LOOKING_FOR_BEGIN:
                if (validator->marker_pos >= begin_len) {
                    if (strstr(validator->marker_buffer, cert_begin)) {
                        validator->state = PEM_STATE_IN_CERTIFICATE;
                        validator->marker_pos = 0; // Reset for end marker
                        ESP_LOGD(TAG, "Found BEGIN marker for certificate %d", validator->cert_count + 1);
                    }
                }
                break;
                
            case PEM_STATE_IN_CERTIFICATE:
                if (validator->marker_pos >= end_len) {
                    if (strstr(validator->marker_buffer, cert_end)) {
                        validator->cert_count++;
                        validator->state = PEM_STATE_LOOKING_FOR_BEGIN;
                        validator->marker_pos = 0; // Reset for next begin marker
                        ESP_LOGD(TAG, "Found END marker for certificate %d", validator->cert_count);
                    }
                }
                break;
                
            default:
                validator->has_error = true;
                break;
        }
    }
}

/**
 * @brief Initialize PEM validator
 */
static void pem_validator_init(pem_validator_t *validator)
{
    memset(validator, 0, sizeof(pem_validator_t));
    validator->state = PEM_STATE_LOOKING_FOR_BEGIN;
}

/**
 * @brief Finalize PEM validation
 */
static bool pem_validator_finalize(pem_validator_t *validator)
{
    if (validator->has_error) {
        ESP_LOGE(TAG, "PEM validation error occurred");
        return false;
    }
    
    if (validator->cert_count == 0) {
        ESP_LOGE(TAG, "No complete certificates found");
        return false;
    }
    
    if (validator->state != PEM_STATE_LOOKING_FOR_BEGIN) {
        ESP_LOGE(TAG, "Incomplete certificate at end of bundle (state: %d)", validator->state);
        return false;
    }
    
    ESP_LOGI(TAG, "PEM validation passed: %d certificates found", validator->cert_count);
    return true;
}

bool cert_bundle_validate_pem(const uint8_t *bundle_data, size_t bundle_size)
{
    if (!bundle_data || bundle_size == 0) {
        return false;
    }

    pem_validator_t validator;
    pem_validator_init(&validator);
    
    // Process data in chunks to simulate streaming validation
    const size_t chunk_size = 512;
    size_t processed = 0;
    
    while (processed < bundle_size && !validator.has_error) {
        size_t remaining = bundle_size - processed;
        size_t current_chunk = (remaining < chunk_size) ? remaining : chunk_size;
        
        pem_validator_process_chunk(&validator, bundle_data + processed, current_chunk);
        processed += current_chunk;
    }
    
    return pem_validator_finalize(&validator);
}

const char* cert_bundle_result_to_string(cert_bundle_result_t result)
{
    switch (result) {
        case CERT_BUNDLE_OK:                return "Success";
        case CERT_BUNDLE_ERROR_INVALID_PARAM: return "Invalid parameter";
        case CERT_BUNDLE_ERROR_PARTITION:   return "Partition error";
        case CERT_BUNDLE_ERROR_MEMORY:      return "Memory allocation error";
        case CERT_BUNDLE_ERROR_TOO_LARGE:   return "Bundle too large";
        case CERT_BUNDLE_ERROR_CRC:         return "CRC validation failed";
        case CERT_BUNDLE_ERROR_WRITE:       return "Write operation failed";
        case CERT_BUNDLE_ERROR_SEMAPHORE:   return "Semaphore error";
        case CERT_BUNDLE_ERROR_UART:        return "UART data collection error";
        default:                            return "Unknown error";
    }
}