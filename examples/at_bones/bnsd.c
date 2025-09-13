/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bnsd.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <errno.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include "ff.h"  // FATFS functions

// Pin assignments for SD card (based on ESP-IDF example)

// #define STEPHAN_BUILD
#ifdef STEPHAN_BUILD
/*
+--------------+----------+-------+
* | SPI Pin | ESP32‑C6 | SD_MMC
+==============+==========+=======+
* | CS (SS) | GPIO18 | DAT3
+--------------+----------+-------+
* | DI (MOSI) | GPIO19 | CMD
+--------------+----------+-------+
* | DO (MISO) | GPIO20 | DAT0
+--------------+----------+-------+
* | SCK (SCLK) | GPIO21 | CLK
+--------------+----------+-------+
*/
#define PIN_NUM_CS    18
#define PIN_NUM_MOSI  19
#define PIN_NUM_CLK   21
#define PIN_NUM_MISO  20
#else
#define PIN_NUM_CS    20
#define PIN_NUM_MOSI  21
#define PIN_NUM_CLK   17
#define PIN_NUM_MISO  16
#endif

static const char *TAG = "BNSD";

// Global SD card context
static struct {
    bool initialized;
    bool mounted;
    char mount_point[32];
    sdmmc_card_t *card;
    esp_vfs_fat_sdmmc_mount_config_t mount_config;
} g_sd_ctx = {0};

bool bnsd_init(void)
{
    if (g_sd_ctx.initialized) {
        ESP_LOGI(TAG, "SD card module already initialized");
        return true;
    }
    
    // Initialize mount point
    strncpy(g_sd_ctx.mount_point, BNSD_MOUNT_POINT, sizeof(g_sd_ctx.mount_point) - 1);
    g_sd_ctx.mount_point[sizeof(g_sd_ctx.mount_point) - 1] = '\0';
    
    // Configure mount options
    g_sd_ctx.mount_config.format_if_mount_failed = false;
    g_sd_ctx.mount_config.max_files = BNSD_MAX_FILES;
    g_sd_ctx.mount_config.allocation_unit_size = 16 * 1024;
    
    g_sd_ctx.initialized = true;
    g_sd_ctx.mounted = false;
    g_sd_ctx.card = NULL;
    
    ESP_LOGI(TAG, "SD card module initialized");
    return true;
}

void bnsd_deinit(void)
{
    if (g_sd_ctx.mounted) {
        bnsd_unmount();
    }
    
    memset(&g_sd_ctx, 0, sizeof(g_sd_ctx));
    ESP_LOGI(TAG, "SD card module deinitialized");
}

bool bnsd_mount(const char *mount_point)
{
    if (!g_sd_ctx.initialized) {
        ESP_LOGE(TAG, "SD card module not initialized");
        return false;
    }
    
    if (g_sd_ctx.mounted) {
        ESP_LOGI(TAG, "SD card already mounted at %s", g_sd_ctx.mount_point);
        return true;
    }
    
    // Use provided mount point or default
    if (mount_point) {
        strncpy(g_sd_ctx.mount_point, mount_point, sizeof(g_sd_ctx.mount_point) - 1);
        g_sd_ctx.mount_point[sizeof(g_sd_ctx.mount_point) - 1] = '\0';
    }
    
    ESP_LOGI(TAG, "Starting adaptive frequency SD card mount at %s", g_sd_ctx.mount_point);
    
    // Adaptive frequency array - start low, increase progressively
    // 100kHz: Safe initialization for all cards
    // 400kHz: SD card specification minimum
    // 1MHz: Low speed but reliable
    // 4MHz: Standard speed  
    // 10MHz: Good performance
    // 20MHz: High performance
    // 26MHz: High performance
    // 32MHz: Maximum for many cards
    // 40MHz: Maximum supported by ESP32
    const uint32_t freq_steps[] = {100, 400, 1000, 4000, 10000, 20000, 26000, 32000, 40000}; // kHz
    const size_t freq_count = sizeof(freq_steps) / sizeof(freq_steps[0]);
    
    ESP_LOGI(TAG, "Using %u frequency steps: 100kHz -> 40MHz", freq_count);
    
    esp_err_t ret = ESP_FAIL;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    
    // Configure SPI bus (done once)
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64 * 1024,  // Large transfers for better throughput
        .flags = SPICOMMON_BUSFLAG_MASTER
    };

    ESP_LOGI(TAG, "Initializing SPI bus with pins: CS=%d, MISO=%d, MOSI=%d, CLK=%d", 
             PIN_NUM_CS, PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK);
    
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Configure SD card slot
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;
    
    // Try mounting at progressively higher frequencies - single loop approach
    bool mount_success = false;
    uint32_t working_freq = 0;
    
    for (size_t i = 0; i < freq_count; i++) {
        host.max_freq_khz = freq_steps[i];
        ESP_LOGI(TAG, "Attempting SD card mount at %u kHz (step %u/%u)", freq_steps[i], i+1, freq_count);
        
        // Clean up previous mount if exists
        if (g_sd_ctx.card) {
            esp_vfs_fat_sdcard_unmount(g_sd_ctx.mount_point, g_sd_ctx.card);
            g_sd_ctx.card = NULL;
        }
        
        // Attempt to mount filesystem at current frequency
        ret = esp_vfs_fat_sdspi_mount(g_sd_ctx.mount_point, &host, &slot_config, 
                                      &g_sd_ctx.mount_config, &g_sd_ctx.card);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "SD card mounted successfully at %u kHz", freq_steps[i]);
            mount_success = true;
            working_freq = freq_steps[i];
            // Continue trying higher frequencies - don't break here
        } else {
            ESP_LOGW(TAG, "Failed to mount at %u kHz: %s", freq_steps[i], esp_err_to_name(ret));
            
            // If we had a working frequency before, revert to it
            if (mount_success && working_freq > 0) {
                ESP_LOGI(TAG, "Reverting to last working frequency: %u kHz", working_freq);
                host.max_freq_khz = working_freq;
                ret = esp_vfs_fat_sdspi_mount(g_sd_ctx.mount_point, &host, &slot_config, 
                                              &g_sd_ctx.mount_config, &g_sd_ctx.card);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "Successfully reverted to %u kHz", working_freq);
                } else {
                    ESP_LOGE(TAG, "Failed to revert to working frequency %u kHz", working_freq);
                    mount_success = false;
                }
                break;
            }
        }
    }
    
    if (!mount_success) {
        ESP_LOGE(TAG, "Failed to mount SD card at any frequency. Final error: %s", esp_err_to_name(ret));
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Filesystem mount failed. Card may need formatting.");
        } else {
            ESP_LOGE(TAG, "Card initialization failed. Check connections and pull-up resistors.");
        }
        spi_bus_free(host.slot);
        return false;
    }
    
    g_sd_ctx.mounted = true;
    
    // Print detailed card info with final operating frequency
    if (g_sd_ctx.card) {
        ESP_LOGI(TAG, "=== SD Card Mount Complete ===");
        ESP_LOGI(TAG, "Final operating frequency: %u kHz", host.max_freq_khz);
        ESP_LOGI(TAG, "Card name: %s", g_sd_ctx.card->cid.name);
        ESP_LOGI(TAG, "Card type: SD Card");
        ESP_LOGI(TAG, "Card speed class: %s", (g_sd_ctx.card->csd.tr_speed > 25000000) ? "High Speed" : "Default Speed");
        
        uint64_t card_size_mb = ((uint64_t) g_sd_ctx.card->csd.capacity) * g_sd_ctx.card->csd.sector_size / (1024 * 1024);
        ESP_LOGI(TAG, "Card capacity: %llu MB (%.2f GB)", card_size_mb, card_size_mb / 1024.0);
        ESP_LOGI(TAG, "Sector size: %u bytes", g_sd_ctx.card->csd.sector_size);
        ESP_LOGI(TAG, "Mount point: %s", g_sd_ctx.mount_point);
        ESP_LOGI(TAG, "============================");
        
        printf("SD Card Info:\n");
        printf("  Name: %s\n", g_sd_ctx.card->cid.name);
        printf("  Frequency: %u kHz\n", host.max_freq_khz);
        printf("  Size: %llu MB\n", card_size_mb);
        printf("  Mount: %s\n", g_sd_ctx.mount_point);
        
        // Performance indicator
        if (host.max_freq_khz >= 32000) {
            printf("  Performance: Excellent (≥32MHz)\n");
        } else if (host.max_freq_khz >= 10000) {
            printf("  Performance: Good (≥10MHz)\n");
        } else if (host.max_freq_khz >= 1000) {
            printf("  Performance: Fair (≥1MHz)\n");
        } else {
            printf("  Performance: Basic (<1MHz)\n");
        }
    }
    
    ESP_LOGI(TAG, "Adaptive frequency mount completed successfully");
    return true;
}

bool bnsd_unmount(void)
{
    if (!g_sd_ctx.initialized) {
        ESP_LOGE(TAG, "SD card module not initialized");
        return false;
    }
    
    if (!g_sd_ctx.mounted) {
        ESP_LOGI(TAG, "SD card not mounted");
        return true;
    }
    
    ESP_LOGI(TAG, "Unmounting SD card from %s", g_sd_ctx.mount_point);
    
    // Log card info before unmount
    if (g_sd_ctx.card) {
        ESP_LOGI(TAG, "Unmounting card: %s", g_sd_ctx.card->cid.name);
    }
    
    // Get the host slot before unmounting
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(g_sd_ctx.mount_point, g_sd_ctx.card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Free the SPI bus
    ESP_LOGI(TAG, "Freeing SPI bus slot %d", host.slot);
    spi_bus_free(host.slot);
    
    g_sd_ctx.mounted = false;
    g_sd_ctx.card = NULL;
    
    ESP_LOGI(TAG, "SD card unmounted successfully");
    return true;
}

bool bnsd_is_mounted(void)
{
    return g_sd_ctx.initialized && g_sd_ctx.mounted;
}

bnsd_status_t bnsd_get_status(void)
{
    if (!g_sd_ctx.initialized) {
        return BNSD_STATUS_ERROR;
    }
    
    return g_sd_ctx.mounted ? BNSD_STATUS_MOUNTED : BNSD_STATUS_UNMOUNTED;
}

bool bnsd_get_space_info(bnsd_info_t *info)
{
    if (!info) {
        ESP_LOGE(TAG, "Invalid info parameter");
        return false;
    }
    
    memset(info, 0, sizeof(bnsd_info_t));
    
    if (!bnsd_is_mounted()) {
        ESP_LOGE(TAG, "SD card not mounted");
        return false;
    }
    
    if (!g_sd_ctx.card) {
        ESP_LOGE(TAG, "SD card context not available");
        return false;
    }
    
    info->is_mounted = true;
    strncpy(info->mount_point, g_sd_ctx.mount_point, sizeof(info->mount_point) - 1);
    info->mount_point[sizeof(info->mount_point) - 1] = '\0';
    
    // Calculate total size from card capacity
    info->total_bytes = ((uint64_t) g_sd_ctx.card->csd.capacity) * g_sd_ctx.card->csd.sector_size;
    
    // For free space, we'll need to use a different approach since statvfs is not available
    // We can try to get filesystem info using FATFS functions
    // For now, we'll estimate based on file system usage
    
    // Try to get free space by checking available clusters
    // This is a simplified approach - in a real implementation you might want to
    // use FATFS f_getfree function or similar
    FATFS *fs;
    DWORD free_clusters;
    
    // Get filesystem object
    if (f_getfree("0:", &free_clusters, &fs) == FR_OK) {
        // Calculate free space (cluster size * sector size = bytes per cluster)
        // Most SD cards use 512 bytes per sector
        uint32_t bytes_per_cluster = fs->csize * 512;
        info->free_bytes = (uint64_t)free_clusters * bytes_per_cluster;
        info->used_bytes = info->total_bytes - info->free_bytes;
    } else {
        // Fallback: assume some reasonable values
        ESP_LOGW(TAG, "Could not get precise free space, using estimates");
        info->free_bytes = info->total_bytes / 2;  // Assume 50% free
        info->used_bytes = info->total_bytes - info->free_bytes;
    }
    
    ESP_LOGI(TAG, "SD Card info:");
    ESP_LOGI(TAG, "  Total: %llu bytes (%.2f MB)", info->total_bytes, info->total_bytes / (1024.0 * 1024.0));
    ESP_LOGI(TAG, "  Used:  %llu bytes (%.2f MB)", info->used_bytes, info->used_bytes / (1024.0 * 1024.0));
    ESP_LOGI(TAG, "  Free:  %llu bytes (%.2f MB)", info->free_bytes, info->free_bytes / (1024.0 * 1024.0));
    
    return true;
}

bool bnsd_mkdir_recursive(const char *path)
{
    if (!path) {
        ESP_LOGE(TAG, "Invalid path parameter");
        return false;
    }
    
    if (!bnsd_is_mounted()) {
        ESP_LOGE(TAG, "SD card not mounted");
        return false;
    }
    
    char full_path[BNSD_MAX_PATH_LENGTH];
    char temp_path[BNSD_MAX_PATH_LENGTH];
    
    // Create full path
    if (path[0] == '/') {
        // Absolute path - check if it starts with mount point
        if (strncmp(path, g_sd_ctx.mount_point, strlen(g_sd_ctx.mount_point)) == 0) {
            strncpy(full_path, path, sizeof(full_path) - 1);
        } else {
            snprintf(full_path, sizeof(full_path), "%s%s", g_sd_ctx.mount_point, path);
        }
    } else {
        // Relative path
        snprintf(full_path, sizeof(full_path), "%s/%s", g_sd_ctx.mount_point, path);
    }
    full_path[sizeof(full_path) - 1] = '\0';
    
    ESP_LOGI(TAG, "Creating directory recursively: %s", full_path);
    
    // Copy path for manipulation
    strncpy(temp_path, full_path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';
    
    // Create directories recursively
    char *p = temp_path;
    
    // Skip the mount point part
    if (strncmp(p, g_sd_ctx.mount_point, strlen(g_sd_ctx.mount_point)) == 0) {
        p += strlen(g_sd_ctx.mount_point);
    }
    
    // Skip leading slash
    if (*p == '/') {
        p++;
    }
    
    while (*p) {
        // Find next directory separator
        char *next_slash = strchr(p, '/');
        if (next_slash) {
            *next_slash = '\0';
        }
        
        // Reconstruct path up to current directory
        char current_path[BNSD_MAX_PATH_LENGTH];
        if (next_slash) {
            size_t current_len = p - temp_path + strlen(p);
            strncpy(current_path, temp_path, current_len);
            current_path[current_len] = '\0';
        } else {
            strncpy(current_path, temp_path, sizeof(current_path) - 1);
            current_path[sizeof(current_path) - 1] = '\0';
        }
        
        // Try to create directory
        struct stat st;
        if (stat(current_path, &st) != 0) {
            if (mkdir(current_path, 0755) != 0) {
                if (errno != EEXIST) {
                    ESP_LOGE(TAG, "Failed to create directory %s: %s", current_path, strerror(errno));
                    return false;
                }
            } else {
                ESP_LOGI(TAG, "Created directory: %s", current_path);
            }
        }
        
        if (next_slash) {
            *next_slash = '/';
            p = next_slash + 1;
        } else {
            break;
        }
    }
    
    ESP_LOGI(TAG, "Directory created successfully: %s", full_path);
    return true;
}

const char *bnsd_get_mount_point(void)
{
    if (bnsd_is_mounted()) {
        return g_sd_ctx.mount_point;
    }
    return NULL;
}

bool bnsd_format(void)
{
    if (!g_sd_ctx.initialized) {
        ESP_LOGE(TAG, "SD card module not initialized");
        return false;
    }
    
    // Card must be mounted before formatting
    bool was_mounted = g_sd_ctx.mounted;
    if (!was_mounted) {
        ESP_LOGI(TAG, "Mounting SD card before formatting (adaptive frequency will be used)");
        if (!bnsd_mount(NULL)) {
            ESP_LOGE(TAG, "Failed to mount SD card before formatting");
            return false;
        }
    }
    
    ESP_LOGI(TAG, "Starting SD card format operation");
    if (g_sd_ctx.card) {
        ESP_LOGI(TAG, "Formatting card: %s", g_sd_ctx.card->cid.name);
    }
    
    // Use the existing mounted card and mount point
    esp_err_t ret = esp_vfs_fat_sdcard_format(g_sd_ctx.mount_point, g_sd_ctx.card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format SD card: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "SD card formatted successfully");
    
    // If the card wasn't originally mounted, unmount it after formatting
    if (!was_mounted) {
        ESP_LOGI(TAG, "Unmounting SD card after format (was not originally mounted)");
        if (!bnsd_unmount()) {
            ESP_LOGW(TAG, "Warning: Failed to unmount SD card after format");
        }
    }
    
    ESP_LOGI(TAG, "SD card format operation completed successfully");
    return true;
}

void bnsd_normalize_path_with_mount_point(char *path, size_t max_length)
{
    if (!path || strlen(path) == 0) {
        return;
    }
    
    // Handle paths starting with @/ or @
    if (path[0] == '@') {
        char temp_path[BNSD_MAX_PATH_LENGTH + 1];
        char *relative_path;
        const char *mount_point;
        
        // Get current mount point (use mounted one or default)
        mount_point = bnsd_get_mount_point();
        if (!mount_point) {
            mount_point = BNSD_MOUNT_POINT;  // Use default if not mounted
        }
        
        // Skip the @ character
        if (path[1] == '/') {
            // @/Downloads -> /MOUNT_POINT/Downloads
            relative_path = &path[2];
        } else {
            // @Downloads -> /MOUNT_POINT/Downloads
            relative_path = &path[1];
        }
        
        // Build the full path with mount point
        int result = snprintf(temp_path, sizeof(temp_path), "%s/%s", 
                             mount_point, relative_path);
        
        if (result >= 0 && result < sizeof(temp_path) && strlen(temp_path) <= max_length) {
            strcpy(path, temp_path);
            ESP_LOGI(TAG, "Normalized path with mount point: %s", path);
        } else {
            ESP_LOGE(TAG, "Path too long after mount point substitution");
        }
    }
}
