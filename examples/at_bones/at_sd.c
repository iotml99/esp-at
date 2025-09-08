/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "at_sd.h"
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

#define STEPHAN_BUILD
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

static const char *TAG = "AT_SD";

// Global SD card context
static struct {
    bool initialized;
    bool mounted;
    char mount_point[32];
    sdmmc_card_t *card;
    esp_vfs_fat_sdmmc_mount_config_t mount_config;
} g_sd_ctx = {0};

bool at_sd_init(void)
{
    if (g_sd_ctx.initialized) {
        ESP_LOGI(TAG, "SD card module already initialized");
        return true;
    }
    
    // Initialize mount point
    strncpy(g_sd_ctx.mount_point, AT_SD_MOUNT_POINT, sizeof(g_sd_ctx.mount_point) - 1);
    g_sd_ctx.mount_point[sizeof(g_sd_ctx.mount_point) - 1] = '\0';
    
    // Configure mount options
    g_sd_ctx.mount_config.format_if_mount_failed = false;
    g_sd_ctx.mount_config.max_files = AT_SD_MAX_FILES;
    g_sd_ctx.mount_config.allocation_unit_size = 16 * 1024;
    
    g_sd_ctx.initialized = true;
    g_sd_ctx.mounted = false;
    g_sd_ctx.card = NULL;
    
    ESP_LOGI(TAG, "SD card module initialized");
    return true;
}

void at_sd_deinit(void)
{
    if (g_sd_ctx.mounted) {
        at_sd_unmount();
    }
    
    memset(&g_sd_ctx, 0, sizeof(g_sd_ctx));
    ESP_LOGI(TAG, "SD card module deinitialized");
}

bool at_sd_mount(const char *mount_point)
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
    
    ESP_LOGI(TAG, "Mounting SD card at %s", g_sd_ctx.mount_point);
    
    // Configure SPI host (following ESP-IDF example pattern)
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 32000;  // Start at 32MHz 

    // Configure SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64 * 1024,  // Large transfers for better throughput
        .flags = SPICOMMON_BUSFLAG_MASTER
    };

    printf("Initializing SPI bus...\n");
    printf("Pins : CS %d, MISO %d, MOSI %d, CLK %d\n", PIN_NUM_CS, PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK);
    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Configure SD card slot
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;
    
    // Mount filesystem
    ret = esp_vfs_fat_sdspi_mount(g_sd_ctx.mount_point, &host, &slot_config, 
                                  &g_sd_ctx.mount_config, &g_sd_ctx.card);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        spi_bus_free(host.slot);
        return false;
    }
    
    g_sd_ctx.mounted = true;
    
    // Print card info
    if (g_sd_ctx.card) {
        ESP_LOGI(TAG, "SD card mounted successfully");
        ESP_LOGI(TAG, "Name: %s", g_sd_ctx.card->cid.name);
        ESP_LOGI(TAG, "Type: SD Card");
        ESP_LOGI(TAG, "Speed: %s", (g_sd_ctx.card->csd.tr_speed > 25000000) ? "high speed" : "default speed");
        ESP_LOGI(TAG, "Size: %lluMB", ((uint64_t) g_sd_ctx.card->csd.capacity) * g_sd_ctx.card->csd.sector_size / (1024 * 1024));
    }
    
    return true;
}

bool at_sd_unmount(void)
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
    
    // Get the host slot before unmounting
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(g_sd_ctx.mount_point, g_sd_ctx.card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Free the SPI bus
    spi_bus_free(host.slot);
    
    g_sd_ctx.mounted = false;
    g_sd_ctx.card = NULL;
    
    ESP_LOGI(TAG, "SD card unmounted successfully");
    return true;
}

bool at_sd_is_mounted(void)
{
    return g_sd_ctx.initialized && g_sd_ctx.mounted;
}

at_sd_status_t at_sd_get_status(void)
{
    if (!g_sd_ctx.initialized) {
        return AT_SD_STATUS_ERROR;
    }
    
    return g_sd_ctx.mounted ? AT_SD_STATUS_MOUNTED : AT_SD_STATUS_UNMOUNTED;
}

bool at_sd_get_space_info(at_sd_info_t *info)
{
    if (!info) {
        ESP_LOGE(TAG, "Invalid info parameter");
        return false;
    }
    
    memset(info, 0, sizeof(at_sd_info_t));
    
    if (!at_sd_is_mounted()) {
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

bool at_sd_mkdir_recursive(const char *path)
{
    if (!path) {
        ESP_LOGE(TAG, "Invalid path parameter");
        return false;
    }
    
    if (!at_sd_is_mounted()) {
        ESP_LOGE(TAG, "SD card not mounted");
        return false;
    }
    
    char full_path[AT_SD_MAX_PATH_LENGTH];
    char temp_path[AT_SD_MAX_PATH_LENGTH];
    
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
        char current_path[AT_SD_MAX_PATH_LENGTH];
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

const char *at_sd_get_mount_point(void)
{
    if (at_sd_is_mounted()) {
        return g_sd_ctx.mount_point;
    }
    return NULL;
}

bool at_sd_format(void)
{
    if (!g_sd_ctx.initialized) {
        ESP_LOGE(TAG, "SD card module not initialized");
        return false;
    }
    
    // Card must be mounted before formatting
    bool was_mounted = g_sd_ctx.mounted;
    if (!was_mounted) {
        ESP_LOGI(TAG, "Mounting SD card before formatting");
        if (!at_sd_mount(NULL)) {
            ESP_LOGE(TAG, "Failed to mount SD card before formatting");
            return false;
        }
    }
    
    ESP_LOGI(TAG, "Starting SD card format operation");
    
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
        if (!at_sd_unmount()) {
            ESP_LOGW(TAG, "Warning: Failed to unmount SD card after format");
        }
    }
    
    ESP_LOGI(TAG, "SD card format operation completed successfully");
    return true;
}
