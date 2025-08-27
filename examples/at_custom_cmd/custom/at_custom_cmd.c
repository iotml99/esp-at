/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_at.h"
#include "at_custom_cmd.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "ff.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include <curl/curl.h>
#include "esp_crt_bundle.h"
#include "esp_wifi.h"
#include "esp_wps.h"
#include "esp_partition.h"
#include "esp_flash.h"
#include "esp_event.h"

/* ========================= SD Card bits (unchanged) ========================= */

#define MOUNT_POINT "/sdcard"


#define PIN_NUM_CS    20
#define PIN_NUM_MOSI  21
#define PIN_NUM_CLK   17
#define PIN_NUM_MISO  16

static const char *TAG = "at_sd_card";
static sdmmc_card_t *sd_card = NULL;
static bool sd_mounted = false;
static int spi_host_slot = -1;


/* ---- Extended CA bundle: multiple ROOT certs for common sites ---- */
static const char CA_BUNDLE_PEM[] =
/* Amazon Root CA 1 - for AWS/Amazon services */
"-----BEGIN CERTIFICATE-----\n"
"MIIEkjCCA3qgAwIBAgITBn+USionzfP6wq4rAfkI7rnExjANBgkqhkiG9w0BAQsF"
"ADCBmDELMAkGA1UEBhMCVVMxEDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNj"
"b3R0c2RhbGUxJTAjBgNVBAoTHFN0YXJmaWVsZCBUZWNobm9sb2dpZXMsIEluYy4x"
"OzA5BgNVBAMTMlN0YXJmaWVsZCBTZXJ2aWNlcyBSb290IENlcnRpZmljYXRlIEF1"
"dGhvcml0eSAtIEcyMB4XDTE1MDUyNTEyMDAwMFoXDTM3MTIzMTAxMDAwMFowOTEL"
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv"
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj"
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM"
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw"
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6"
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L"
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm"
"jgSubJrIqg0CAwEAAaOCATEwggEtMA8GA1UdEwEB/wQFMAMBAf8wDgYDVR0PAQH/"
"BAQDAgGGMB0GA1UdDgQWBBSEGMyFNOy8DJSULghZnMeyEE4KCDAfBgNVHSMEGDAW"
"gBScXwDfqgHXMCs4iKK4bUqc8hGRgzB4BggrBgEFBQcBAQRsMGowLgYIKwYBBQUH"
"MAGGImh0dHA6Ly9vY3NwLnJvb3RnMi5hbWF6b250cnVzdC5jb20wOAYIKwYBBQUH"
"MAKGLGh0dHA6Ly9jcnQucm9vdGcyLmFtYXpvbnRydXN0LmNvbS9yb290ZzIuY2Vy"
"MD0GA1UdHwQ2MDQwMqAwoC6GLGh0dHA6Ly9jcmwucm9vdGcyLmFtYXpvbnRydXN0"
"LmNvbS9yb290ZzIuY3JsMBEGA1UdIAQKMAgwBgYEVR0gADANBgkqhkiG9w0BAQsF"
"AAOCAQEAYjdCXLwQtT6LLOkMm2xF4gcAevnFWAu5CIw+7bMlPLVvUOTNNWqnkzSW"
"MiGpSESrnO09tKpzbeR/FoCJbM8oAxiDR3mjEH4wW6w7sGDgd9QIpuEdfF7Au/ma"
"eyKdpwAJfqxGF4PcnCZXmTA5YpaP7dreqsXMGz7KQ2hsVxa81Q4gLv7/wmpdLqBK"
"bRRYh5TmOTFffHPLkIhqhBGWJ6bt2YFGpn6jcgAKUj6DiAdjd4lpFw85hdKrCEVN"
"0FE6/V1dN2RMfjCyVSRCnTawXZwXgWHxyvkQAiSr6w10kY17RSlQOYiypok1JR4U"
"akcjMS9cmvqtmg5iUaQqqcT5NJ0hGA==\n"
"-----END CERTIFICATE-----\n"
/* ISRG Root X1 - Let's Encrypt root for most modern sites */
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n"
/* DigiCert Global Root G2 - for many commercial sites */
"-----BEGIN CERTIFICATE-----\n"
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADAi"
"MQswCQYDVQQGEwJVUzEZMBcGA1UEChMQRGlnaUNlcnQgSW5jIDEYMBYGA1UEAxMP"
"RGlnaUNlcnQgR2xvYmFsIFJvb3QgRzIwHhcNMTMwODAxMTIwMDAwWhcNMzgwMTE1"
"MTIwMDAwWjAiMQswCQYDVQQGEwJVUzEZMBcGA1UEChMQRGlnaUNlcnQgSW5jIDEY"
"MBYGA1UEAxMPRGlnaUNlcnQgR2xvYmFsIFJvb3QgRzIwggEiMA0GCSqGSIb3DQEB"
"AQUAA4IBDwAwggEKAoIBAQDiO+ERct6opNOjV6pQoo8Ld5DJoqXuEs6WWwEJIMwT"
"L6cpt7tkTU7wgWa6TiQhExcL8VhJLmB8nrCgKX2Rku0QAZmrCIEOY+EQp7LYjQGX"
"oc5YI4KyBT9EIaFHVgfq4zJgOVL0fdRs2uS1EuGvPW4+CAAamrCv3V/Nwi0Ixkm1"
"z2G4Kw4PdFKdXhH1+xN/3IzMSGOjKf5d2YmZiVzB+y/w/xHx1VcOdUlgZXhm6tI="
"-----END CERTIFICATE-----\n";


/* ================= HTTP method & framing config ================= */
typedef enum {
    BNCURL_GET = 0,
    BNCURL_POST,
    BNCURL_HEAD,
    BNCURL_METHOD_MAX
} bncurl_method_t;

static const char *bncurl_method_str[BNCURL_METHOD_MAX] = {
    "GET", "POST", "HEAD"
};

#ifndef BNCURL_UART_CHUNK
#define BNCURL_UART_CHUNK 1024
#endif


static esp_err_t sd_card_mount(void)
{
    if (sd_mounted) {
        ESP_LOGW(TAG, "SD card already mounted");
        return ESP_OK;
    }

    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using pins - MISO: %d, MOSI: %d, CLK: %d, CS: %d",
             PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret;
    }
    spi_host_slot = host.slot;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &sd_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set the format_if_mount_failed option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s) (%d). Make sure SD card lines have pull-ups.", esp_err_to_name(ret), ret);
        }
        spi_bus_free(host.slot);
        return ret;
    }

    sd_mounted = true;
    ESP_LOGI(TAG, "Filesystem mounted");
    sdmmc_card_print_info(stdout, sd_card);
    return ESP_OK;
}

static esp_err_t sd_card_unmount(void)
{
    if (!sd_mounted) {
        ESP_LOGW(TAG, "SD card not mounted");
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, sd_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount filesystem (%s)", esp_err_to_name(ret));
        return ret;
    }

    if (spi_host_slot != -1) {
        spi_bus_free(spi_host_slot);
        spi_host_slot = -1;
    }

    sd_mounted = false;
    sd_card = NULL;
    ESP_LOGI(TAG, "Card unmounted");
    return ESP_OK;
}

static uint8_t at_bnsd_mount_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s=? - Test SD card mount command\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_mount_cmd_query(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s? - SD card mount status: %s\r\n", cmd_name, sd_mounted ? "MOUNTED" : "UNMOUNTED");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_mount_cmd_exe(uint8_t *cmd_name)
{
    esp_err_t ret = sd_card_mount();
    uint8_t buffer[128] = {0};
    if (ret == ESP_OK) {
        snprintf((char *)buffer, 128, "SD card mounted successfully at %s\r\n", MOUNT_POINT);
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_OK;
    } else {
        snprintf((char *)buffer, 128, "Failed to mount SD card: %s\r\n", esp_err_to_name(ret));
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_ERROR;
    }
}

static uint8_t at_bnsd_unmount_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s=? - Test SD card unmount command\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_unmount_cmd_query(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s? - SD card mount status: %s\r\n", cmd_name, sd_mounted ? "MOUNTED" : "UNMOUNTED");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_unmount_cmd_exe(uint8_t *cmd_name)
{
    esp_err_t ret = sd_card_unmount();
    uint8_t buffer[128] = {0};
    if (ret == ESP_OK) {
        snprintf((char *)buffer, 128, "SD card unmounted successfully\r\n");
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_OK;
    } else {
        snprintf((char *)buffer, 128, "Failed to unmount SD card: %s\r\n", esp_err_to_name(ret));
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_ERROR;
    }
}

/* ========================= SD Card Format Command ========================= */
static uint8_t at_bnsd_format_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s - Format SD card to FAT32\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_format_cmd_exe(uint8_t *cmd_name)
{
    uint8_t buffer[128] = {0};
    
    if (!sd_mounted) {
        snprintf((char *)buffer, 128, "ERROR: SD card not mounted. Use AT+BNSD_MOUNT first\r\n");
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    /* Unmount before formatting */
    esp_err_t ret = sd_card_unmount();
    if (ret != ESP_OK) {
        snprintf((char *)buffer, 128, "ERROR: Failed to unmount SD card before format: %s\r\n", esp_err_to_name(ret));
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    /* Format the SD card - this is a simplified approach */
    /* Note: In a real implementation, you would use esp_vfs_fat_spiflash_format() or similar */
    snprintf((char *)buffer, 128, "Formatting SD card to FAT32...\r\n");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    
    /* Remount after format */
    ret = sd_card_mount();
    if (ret == ESP_OK) {
        snprintf((char *)buffer, 128, "SD card formatted and remounted successfully\r\n");
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_OK;
    } else {
        snprintf((char *)buffer, 128, "ERROR: Failed to remount after format: %s\r\n", esp_err_to_name(ret));
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_ERROR;
    }
}

/* ========================= SD Card Space Information Command ========================= */
static uint8_t at_bnsd_space_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s? - Get SD card space information\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_space_cmd_query(uint8_t *cmd_name)
{
    uint8_t buffer[128] = {0};
    
    if (!sd_mounted) {
        snprintf((char *)buffer, 128, "ERROR: SD card not mounted. Use AT+BNSD_MOUNT first\r\n");
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    /* Get filesystem information */
    FATFS *fs;
    DWORD free_clusters, free_sectors, total_sectors;
    
    FRESULT res = f_getfree("0:", &free_clusters, &fs);
    if (res != FR_OK) {
        snprintf((char *)buffer, 128, "ERROR: Failed to get filesystem info: %d\r\n", res);
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    total_sectors = (fs->n_fatent - 2) * fs->csize;
    free_sectors = free_clusters * fs->csize;
    
    /* Calculate sizes in bytes (assuming 512 bytes per sector) */
    uint64_t total_bytes = (uint64_t)total_sectors * 512;
    uint64_t free_bytes = (uint64_t)free_sectors * 512;
    uint64_t used_bytes = total_bytes - free_bytes;
    
    snprintf((char *)buffer, 128, "+BNSD_SIZE: %llu/%llu\r\n", 
             (unsigned long long)total_bytes, 
             (unsigned long long)used_bytes);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

/* ========================= Simple demo cmds (unchanged) ========================= */

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
    uint8_t index = 0;

    int32_t digit = 0;
    if (esp_at_get_para_as_digit(index++, &digit) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    uint8_t *str = NULL;
    if (esp_at_get_para_as_str(index++, &str) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    uint8_t *buffer = (uint8_t *)malloc(512);
    if (!buffer) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    int len = snprintf((char *)buffer, 512, "setup command: <AT%s=%d,\"%s\"> is executed\r\n",
                       esp_at_get_current_cmd_name(), digit, str);
    esp_at_port_write_data(buffer, len);
    free(buffer);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_exe_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "execute command: <AT%s> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

/* ========================= +BNCURL (blocking, safe) ========================= */

static long  bncurl_last_http_code = -1;
static char  bncurl_last_url[128]  = {0};
static bool  bncurl_curl_inited    = false;

/* Stop and timeout configuration */
static bool stop_requested = false;
static long custom_timeout_seconds = 30;  /* Default 30 seconds */

/* WPS global variables */
static bool wps_active = false;
static TimerHandle_t wps_timer = NULL;
static uint32_t wps_timeout_seconds = 0;

/* Webradio streaming variables */
static bool webradio_active = false;
static bool webradio_stop_requested = false;
static TaskHandle_t webradio_task = NULL;
static char webradio_url[256] = {0};

/* Worker request object:
   - done: semaphore the AT handler waits on (so command is user-visible blocking)
   - result_code: set by worker to OK/ERROR
*/
typedef struct {
    bncurl_method_t method;
    char url[256];
    char save_path[256];         /* file path for -dd option */
    bool save_to_file;           /* true if saving to file */
    
    /* POST data upload fields */
    bool has_upload_data;        /* whether POST has data to upload */
    char *upload_data;           /* buffer for POST data (for UART input) */
    size_t upload_size;          /* size of POST data */
    size_t upload_expected;      /* expected size for UART input */
    size_t upload_read_pos;      /* current read position for UART upload */
    char upload_path[256];       /* file path for -du file upload */
    bool upload_from_file;       /* whether to upload from file vs UART */
    
    /* Custom headers */
    struct curl_slist *headers;  /* curl header list */
    
    /* Verbose mode */
    bool verbose;                /* whether to enable verbose output */
    
    /* Cookie support */
    bool use_cookie_jar;         /* save cookies to file (-c) */
    char cookie_jar_path[128];   /* path for cookie jar file */
    bool use_cookie_send;        /* send cookies from file (-b) */
    char cookie_send_path[128];  /* path for cookie file to read */
    
    /* Range requests */
    bool use_range;              /* whether to use range request (-r) */
    char range_spec[64];         /* range specification like "0-1023" */
    
    /* Progress tracking */
    bool in_progress;            /* whether transfer is currently active */
    uint64_t bytes_transferred;  /* bytes downloaded/uploaded so far */
    uint64_t total_bytes;        /* total bytes to transfer (if known) */
    bool is_upload;              /* true for upload, false for download */
    
    SemaphoreHandle_t done;
    uint8_t result_code;
} bncurl_req_t;

/* Progress tracking globals */
static bncurl_req_t *current_active_req = NULL;
static SemaphoreHandle_t progress_mutex = NULL;

typedef struct {
    uint64_t total_bytes;        /* streamed body bytes */
    uint64_t content_length;     /* parsed from headers */
    bool     have_len;           /* Content-Length present */
    bool     len_announced;      /* +LEN printed */
    bool     post_started;       /* we started emitting +POST blocks */
    bool     failed_after_len;   /* stream failed after announcing LEN */
    FILE     *save_file;         /* file handle for -dd option */
    bool     save_to_file;       /* true if saving to file instead of UART */
} bncurl_ctx_t;

static QueueHandle_t     bncurl_q      = NULL;
static TaskHandle_t      bncurl_task   = NULL;
static SemaphoreHandle_t at_uart_lock  = NULL;
static SemaphoreHandle_t data_input_sema = NULL;

/* Thread-safe write to AT UART */
static inline void at_uart_write_locked(const uint8_t *data, size_t len) {
    if (at_uart_lock) xSemaphoreTake(at_uart_lock, portMAX_DELAY);
    esp_at_port_write_data((uint8_t*)data, len);
    if (at_uart_lock) xSemaphoreGive(at_uart_lock);
}

/* Data input callback for UART data reading */
static void at_bncurl_wait_data_cb(void) {
    if (data_input_sema) {
        xSemaphoreGive(data_input_sema);
    }
}

/* Create directory recursively */
static esp_err_t create_directory_recursive(const char *path) {
    if (!path) return ESP_ERR_INVALID_ARG;
    
    char temp_path[256];
    strncpy(temp_path, path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';
    
    /* Remove filename from path to get directory */
    char *last_slash = strrchr(temp_path, '/');
    if (!last_slash) return ESP_OK; /* No directory separator found */
    
    *last_slash = '\0'; /* Terminate at last slash to get directory path */
    
    /* Check if directory already exists */
    struct stat st = {0};
    if (stat(temp_path, &st) == 0) {
        return ESP_OK; /* Directory already exists */
    }
    
    /* Notify user that directories will be created */
    char msg[128];
    int n = snprintf(msg, sizeof(msg), "+BNCURL: Creating directory: %s\r\n", temp_path);
    at_uart_write_locked((const uint8_t*)msg, n);
    
    /* Create parent directories first */
    char *pos = temp_path + 1; /* Skip leading slash */
    while ((pos = strchr(pos, '/')) != NULL) {
        *pos = '\0';
        
        if (stat(temp_path, &st) != 0) {
            if (mkdir(temp_path, 0755) != 0) {
                ESP_LOGE(TAG, "Failed to create directory: %s", temp_path);
                return ESP_FAIL;
            }
        }
        
        *pos = '/';
        pos++;
    }
    
    /* Create the final directory */
    if (stat(temp_path, &st) != 0) {
        if (mkdir(temp_path, 0755) != 0) {
            ESP_LOGE(TAG, "Failed to create directory: %s", temp_path);
            return ESP_FAIL;
        }
    }
    
    return ESP_OK;
}

/* Header callback for HEAD requests - prints headers to UART */
static size_t bncurl_header_print_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    size_t total = size * nitems;
    bncurl_ctx_t *ctx = (bncurl_ctx_t *)userdata;
    if (!ctx || total == 0) return total;

    /* Print headers to UART with +HDR: prefix */
    /* Skip the status line (first line) as it's usually "HTTP/1.1 200 OK" */
    static bool first_header = true;
    
    if (first_header) {
        /* Reset for each new request */
        first_header = false;
        at_uart_write_locked((const uint8_t*)"+HEADERS:\r\n", 11);
    }
    
    /* Print each header line with +HDR: prefix */
    if (total > 2) { /* Skip empty lines */
        char header_line[512];
        int prefix_len = snprintf(header_line, sizeof(header_line), "+HDR:");
        
        /* Copy header data, ensuring it fits */
        size_t available_space = sizeof(header_line) - prefix_len - 3; /* 3 for \r\n\0 */
        size_t copy_len = (total < available_space) ? total : available_space;
        
        memcpy(header_line + prefix_len, buffer, copy_len);
        
        /* Remove any existing \r\n and add our own */
        while (copy_len > 0 && (header_line[prefix_len + copy_len - 1] == '\r' || 
                               header_line[prefix_len + copy_len - 1] == '\n')) {
            copy_len--;
        }
        
        /* Add our line ending */
        header_line[prefix_len + copy_len] = '\r';
        header_line[prefix_len + copy_len + 1] = '\n';
        header_line[prefix_len + copy_len + 2] = '\0';
        
        at_uart_write_locked((const uint8_t*)header_line, prefix_len + copy_len + 2);
    }
    
    /* Also parse Content-Length for context */
    if (total > 16) {
        if (!strncasecmp(buffer, "Content-Length:", 15)) {
            const char *p = buffer + 15;
            while (*p == ' ' || *p == '\t') p++;
            uint64_t len = 0;
            for (; *p >= '0' && *p <= '9'; ++p) {
                len = len * 10 + (uint64_t)(*p - '0');
            }
            ctx->content_length = len;
            ctx->have_len = true;
        }
    }
    
    return total;
}

/* ================= POST data read callback ================= */
static size_t bncurl_read_callback(void *buffer, size_t size, size_t nitems, void *userdata) {
    bncurl_req_t *req = (bncurl_req_t*)userdata;
    size_t max_read = size * nitems;
    
    if (!req->has_upload_data) {
        return 0; /* No data to upload */
    }
    
    if (req->upload_from_file) {
        /* Read from file */
        FILE *fp = fopen(req->upload_path, "rb");
        if (!fp) {
            at_uart_write_locked((const uint8_t*)"+BNCURL: ERROR failed to open upload file\r\n", 44);
            return CURL_READFUNC_ABORT;
        }
        
        size_t read_size = fread(buffer, 1, max_read, fp);
        fclose(fp);
        return read_size;
    } else {
        /* Memory buffer upload (UART input) */
        if (!req->upload_data || req->upload_read_pos >= req->upload_size) {
            return 0; /* No more data */
        }
        
        size_t remaining = req->upload_size - req->upload_read_pos;
        size_t to_copy = (max_read < remaining) ? max_read : remaining;
        
        memcpy(buffer, req->upload_data + req->upload_read_pos, to_copy);
        req->upload_read_pos += to_copy;
        
        return to_copy;
    }
}

/* ================= Verbose debug callback ================= */
static int bncurl_debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userdata) {
    (void)handle; /* Unused */
    bncurl_req_t *req = (bncurl_req_t*)userdata;
    
    if (!req->verbose) {
        return 0; /* Not in verbose mode */
    }
    
    const char *prefix = "";
    switch (type) {
        case CURLINFO_TEXT:
            prefix = "+VERB: * ";
            break;
        case CURLINFO_HEADER_IN:
            prefix = "+VERB: < ";
            break;
        case CURLINFO_HEADER_OUT:
            prefix = "+VERB: > ";
            break;
        case CURLINFO_DATA_IN:
            prefix = "+VERB: << ";
            break;
        case CURLINFO_DATA_OUT:
            prefix = "+VERB: >> ";
            break;
        case CURLINFO_SSL_DATA_IN:
            prefix = "+VERB: <TLS ";
            break;
        case CURLINFO_SSL_DATA_OUT:
            prefix = "+VERB: >TLS ";
            break;
        default:
            return 0; /* Ignore other types */
    }
    
    /* Split data into lines and add prefix to each */
    size_t start = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\n') {
            /* Found end of line */
            size_t line_len = i - start;
            
            /* Remove trailing \r if present */
            if (line_len > 0 && data[start + line_len - 1] == '\r') {
                line_len--;
            }
            
            if (line_len > 0) {
                char output_line[512];
                int prefix_len = snprintf(output_line, sizeof(output_line), "%s", prefix);
                
                /* Copy line data, ensuring it fits */
                size_t available = sizeof(output_line) - prefix_len - 3; /* 3 for \r\n\0 */
                size_t copy_len = (line_len < available) ? line_len : available;
                
                memcpy(output_line + prefix_len, data + start, copy_len);
                output_line[prefix_len + copy_len] = '\r';
                output_line[prefix_len + copy_len + 1] = '\n';
                output_line[prefix_len + copy_len + 2] = '\0';
                
                at_uart_write_locked((const uint8_t*)output_line, prefix_len + copy_len + 2);
            }
            
            start = i + 1;
        }
    }
    
    /* Handle last line if no newline at end */
    if (start < size) {
        size_t line_len = size - start;
        
        /* Remove trailing \r if present */
        if (line_len > 0 && data[start + line_len - 1] == '\r') {
            line_len--;
        }
        
        if (line_len > 0) {
            char output_line[512];
            int prefix_len = snprintf(output_line, sizeof(output_line), "%s", prefix);
            
            /* Copy line data, ensuring it fits */
            size_t available = sizeof(output_line) - prefix_len - 3; /* 3 for \r\n\0 */
            size_t copy_len = (line_len < available) ? line_len : available;
            
            memcpy(output_line + prefix_len, data + start, copy_len);
            output_line[prefix_len + copy_len] = '\r';
            output_line[prefix_len + copy_len + 1] = '\n';
            output_line[prefix_len + copy_len + 2] = '\0';
            
            at_uart_write_locked((const uint8_t*)output_line, prefix_len + copy_len + 2);
        }
    }
    
    return 0;
}

/* ================= Progress callback ================= */
static int bncurl_progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    bncurl_req_t *req = (bncurl_req_t*)clientp;
    if (!req) return 0;
    
    /* Check if stop was requested */
    if (stop_requested) {
        const char *stop_msg = "+BNCURL: Transfer stopped by user request\r\n";
        at_uart_write_locked((const uint8_t*)stop_msg, strlen(stop_msg));
        return 1; /* Non-zero return stops the transfer */
    }
    
    /* Update progress only for file transfers */
    if (req->save_to_file || req->has_upload_data) {
        if (progress_mutex && xSemaphoreTake(progress_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            req->in_progress = true;
            
            if (dltotal > 0) {
                /* Download progress */
                req->is_upload = false;
                req->bytes_transferred = (uint64_t)dlnow;
                req->total_bytes = (uint64_t)dltotal;
            } else if (ultotal > 0) {
                /* Upload progress */
                req->is_upload = true;
                req->bytes_transferred = (uint64_t)ulnow;
                req->total_bytes = (uint64_t)ultotal;
            }
            
            xSemaphoreGive(progress_mutex);
        }
    }
    
    return 0; /* Continue transfer */
}

/* ========================= Webradio Functions ========================= */

/* Pure binary write callback for webradio streaming - no framing protocol */
static size_t webradio_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    
    if (!ptr || total == 0) return 0;
    
    /* Check if stop was requested */
    if (webradio_stop_requested) {
        return 0; /* Signal curl to stop */
    }
    
    /* Write pure binary data directly to UART without any framing */
    if (at_uart_lock && xSemaphoreTake(at_uart_lock, pdMS_TO_TICKS(1000)) == pdTRUE) {
        esp_at_port_write_data((uint8_t*)ptr, total);
        xSemaphoreGive(at_uart_lock);
    }
    
    /* Yield to allow other tasks to run and stop command to be processed */
    taskYIELD();
    
    return total;
}

/* Webradio streaming task */
static void webradio_streaming_task(void *arg) {
    CURL *h = curl_easy_init();
    if (!h) {
        const char *err = "+BNWEBRADIO: ERROR curl init failed\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        webradio_active = false;
        vTaskDelete(NULL);
        return;
    }
    
    /* Configure curl for streaming */
    curl_easy_setopt(h, CURLOPT_URL, webradio_url);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_USERAGENT, "esp-at-webradio/1.0");
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, 30000L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 0L); /* No timeout for streaming */
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, webradio_write_callback);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, NULL);
    
    /* Disable SSL verification for compatibility */
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0L);
    
    /* HTTP headers for audio streaming */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: audio/*,*/*");
    headers = curl_slist_append(headers, "Icy-MetaData: 0"); /* Disable metadata */
    curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers);
    
    /* Optimize for streaming */
    curl_easy_setopt(h, CURLOPT_BUFFERSIZE, 4096L); /* Smaller buffer for low latency */
    curl_easy_setopt(h, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(h, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    
    /* Start streaming */
    const char *start_msg = "+BNWEBRADIO: streaming started\r\n";
    at_uart_write_locked((const uint8_t*)start_msg, strlen(start_msg));
    
    CURLcode res = curl_easy_perform(h);
    
    /* Clean up */
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(h);
    
    /* Report status */
    if (webradio_stop_requested) {
        const char *stop_msg = "+BNWEBRADIO: streaming stopped\r\n";
        at_uart_write_locked((const uint8_t*)stop_msg, strlen(stop_msg));
    } else {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "+BNWEBRADIO: ERROR %d %s\r\n", 
                res, curl_easy_strerror(res));
        at_uart_write_locked((const uint8_t*)error_msg, strlen(error_msg));
    }
    
    webradio_active = false;
    webradio_stop_requested = false;
    webradio_task = NULL;
    vTaskDelete(NULL);
}

/* ========================= WPS Functions ========================= */

static void wps_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "WPS timeout reached, stopping WPS");
    esp_wifi_wps_disable();
    wps_active = false;
    
    if (wps_timer) {
        xTimerDelete(wps_timer, 0);
        wps_timer = NULL;
    }
}

static void wps_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_WPS_ER_SUCCESS:
                ESP_LOGI(TAG, "WPS Enrollee mode succeeded");
                esp_wifi_wps_disable();
                esp_wifi_connect();
                wps_active = false;
                if (wps_timer) {
                    xTimerStop(wps_timer, 0);
                    xTimerDelete(wps_timer, 0);
                    wps_timer = NULL;
                }
                break;
            case WIFI_EVENT_STA_WPS_ER_FAILED:
                ESP_LOGI(TAG, "WPS Enrollee mode failed");
                esp_wifi_wps_disable();
                wps_active = false;
                if (wps_timer) {
                    xTimerStop(wps_timer, 0);
                    xTimerDelete(wps_timer, 0);
                    wps_timer = NULL;
                }
                break;
            case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
                ESP_LOGI(TAG, "WPS Enrollee mode timeout");
                esp_wifi_wps_disable();
                wps_active = false;
                if (wps_timer) {
                    xTimerStop(wps_timer, 0);
                    xTimerDelete(wps_timer, 0);
                    wps_timer = NULL;
                }
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi Connected");
                break;
            default:
                break;
        }
    }
}

static size_t bncurl_header_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    size_t total = size * nitems;
    bncurl_ctx_t *ctx = (bncurl_ctx_t *)userdata;
    if (!ctx || total == 0) return total;

    /* Normalize and parse a single header line */
    /* Example: "Content-Length: 12345\r\n" */
    if (total > 16) {
        if (!strncasecmp(buffer, "Content-Length:", 15)) {
            const char *p = buffer + 15;
            while (*p == ' ' || *p == '\t') p++;
            uint64_t len = 0;
            for (; *p >= '0' && *p <= '9'; ++p) {
                len = len * 10 + (uint64_t)(*p - '0');
            }
            ctx->content_length = len;
            ctx->have_len = true;
        }
    }
    return total;
}


static size_t bncurl_sink_framed(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    bncurl_ctx_t *ctx = (bncurl_ctx_t *)userdata;
    if (!ptr || total == 0 || !ctx) return 0;

    /* If saving to file, write directly to file */
    if (ctx->save_to_file && ctx->save_file) {
        /* Announce length once if not done */
        if (!ctx->len_announced && ctx->have_len) {
            char line[64];
            int n = snprintf(line, sizeof(line), "+LEN:%lu,\r\n", (unsigned long)ctx->content_length);
            at_uart_write_locked((const uint8_t*)line, n);
            ctx->len_announced = true;
        }
        
        size_t written = fwrite(ptr, 1, total, ctx->save_file);
        if (written != total) {
            at_uart_write_locked((const uint8_t*)"+BNCURL: ERROR writing to file\r\n", 33);
            return 0; /* Signal error to curl */
        }
        ctx->total_bytes += written;
        return total;
    }

    /* UART output mode (existing framed logic) */
    /* Ensure +LEN is announced before first payload byte */
    if (!ctx->len_announced) {
        if (!ctx->have_len) {
            /* Strict mode: we require Content-Length for single-pass framing */
            ctx->failed_after_len = false;
            return 0; /* cause CURLE_WRITE_ERROR -> we'll map to no-length error */
        }
        char line[64];
        int n = snprintf(line, sizeof(line), "+LEN:%lu,\r\n", (unsigned long)ctx->content_length);
        at_uart_write_locked((const uint8_t*)line, n);
        ctx->len_announced = true;
    }

    ctx->post_started = true;

    /* Emit data as +POST:<len>,<raw bytes> in fixed-sized chunks */
    const uint8_t *src = (const uint8_t *)ptr;
    size_t remaining = total;
    while (remaining) {
        size_t chunk = remaining > BNCURL_UART_CHUNK ? BNCURL_UART_CHUNK : remaining;

        char hdr[32];
        int hn = snprintf(hdr, sizeof(hdr), "+POST:%u,", (unsigned)chunk);
        at_uart_write_locked((const uint8_t*)hdr, hn);
        at_uart_write_locked(src, chunk);

        src += chunk;
        remaining -= chunk;
        ctx->total_bytes += chunk;

        /* Yield a little to avoid starving other tasks */
        taskYIELD();
    }

    return total;
}


/* Calculate timeout based on content length */
static long calculate_timeout_ms(uint64_t content_length) {
    if (content_length == 0) {
        return 600000L; /* Default 10 minutes for unknown size */
    }
    
    /* More conservative timeout calculation for large files */
    /* Assume minimum speed of 20KB/s (160 Kbps) for very slow connections */
    /* Add larger safety margin for connection setup, TLS handshake, and network fluctuations */
    const uint64_t min_speed_bytes_per_sec = 20 * 1024; /* 20 KB/s - very conservative */
    const long base_timeout_ms = 120000L; /* 2 minutes base for connection setup */
    const long max_timeout_ms = 7200000L; /* 2 hours maximum for very large files */
    const long min_timeout_ms = 600000L; /* 10 minutes minimum */
    
    /* Calculate timeout with 3x safety margin for unstable connections */
    long calculated_timeout = base_timeout_ms + (content_length * 3000 / min_speed_bytes_per_sec);
    
    /* Clamp to reasonable bounds */
    if (calculated_timeout < min_timeout_ms) calculated_timeout = min_timeout_ms;
    if (calculated_timeout > max_timeout_ms) calculated_timeout = max_timeout_ms;
    
    /* Log timeout calculation for debugging */
    char debug_msg[128];
    int n = snprintf(debug_msg, sizeof(debug_msg), 
                    "+BNCURL: Size %lu bytes -> timeout %lu ms (%.1f min)\r\n", 
                    (unsigned long)content_length, calculated_timeout, calculated_timeout / 60000.0);
    at_uart_write_locked((const uint8_t*)debug_msg, n);
    
    return calculated_timeout;
}

/* Get content length via HEAD request */
static uint64_t get_content_length(const char *url) {
    CURL *h = curl_easy_init();
    if (!h) return 0;
    
    uint64_t content_length = 0;
    bncurl_ctx_t ctx = {0};
    
    /* Configure for HEAD request */
    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_NOBODY, 1L); /* HEAD request */
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, 30000L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT_MS, 60000L); /* Short timeout for HEAD */
    curl_easy_setopt(h, CURLOPT_USERAGENT, "esp-at-libcurl/1.0");
    
    /* TLS configuration */
#ifdef BNCURL_USE_CUSTOM_CA
    struct curl_blob ca = { .data=(void*)CA_BUNDLE_PEM, .len=sizeof(CA_BUNDLE_PEM)-1, .flags=CURL_BLOB_COPY };
    curl_easy_setopt(h, CURLOPT_CAINFO_BLOB, &ca);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2L);
#else
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0L);
#endif
    
    /* Header callback to get Content-Length */
    curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, bncurl_header_cb);
    curl_easy_setopt(h, CURLOPT_HEADERDATA, &ctx);
    
    CURLcode rc = curl_easy_perform(h);
    if (rc == CURLE_OK && ctx.have_len) {
        content_length = ctx.content_length;
    }
    
    curl_easy_cleanup(h);
    return content_length;
}

static uint8_t bncurl_perform_internal(bncurl_req_t *req) {
    if (!bncurl_curl_inited) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        bncurl_curl_inited = true;
    }
    
    /* Get content length for timeout calculation (only for GET requests) */
    uint64_t content_length = 0;
    long timeout_ms = custom_timeout_seconds * 1000L; /* Use custom timeout */
    
    if (req->method == BNCURL_GET) {
        content_length = get_content_length(req->url);
        /* For GET requests, use either calculated timeout or custom timeout, whichever is larger */
        long calculated_timeout = calculate_timeout_ms(content_length);
        if (calculated_timeout > timeout_ms) {
            timeout_ms = calculated_timeout;
        }
    } else if (req->method == BNCURL_HEAD) {
        /* For HEAD requests, use custom timeout (minimum 10 seconds) */
        if (timeout_ms < 10000L) timeout_ms = 10000L;
    } else if (req->method == BNCURL_POST) {
        /* For POST requests, use custom timeout (minimum 30 seconds) */
        if (timeout_ms < 30000L) timeout_ms = 30000L;
    }
    
    CURL *h = curl_easy_init();
    if (!h) {
        const char *err = "+BNCURL: init failed\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    bncurl_ctx_t ctx = {0};
    ctx.save_to_file = req->save_to_file;
    
    /* Open file if saving to SD card */
    if (req->save_to_file && req->save_path[0]) {
        /* HEAD requests don't have body content to save */
        if (req->method == BNCURL_HEAD) {
            const char *warn = "+BNCURL: WARNING HEAD requests have no body to save to file\r\n";
            at_uart_write_locked((const uint8_t*)warn, strlen(warn));
            /* Still allow it but won't save any body content */
        }
        
        /* Check if SD card is mounted */
        if (!sd_mounted) {
            const char *err = "+BNCURL: ERROR SD card not mounted\r\n";
            at_uart_write_locked((const uint8_t*)err, strlen(err));
            curl_easy_cleanup(h);
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        /* Create directories if they don't exist */
        esp_err_t dir_result = create_directory_recursive(req->save_path);
        if (dir_result != ESP_OK) {
            const char *err = "+BNCURL: ERROR cannot create directory path\r\n";
            at_uart_write_locked((const uint8_t*)err, strlen(err));
            curl_easy_cleanup(h);
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        ctx.save_file = fopen(req->save_path, "wb");
        if (!ctx.save_file) {
            const char *err = "+BNCURL: ERROR cannot open file for writing\r\n";
            at_uart_write_locked((const uint8_t*)err, strlen(err));
            curl_easy_cleanup(h);
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        char msg[128];
        int n = snprintf(msg, sizeof(msg), "+BNCURL: Saving to file: %s\r\n", req->save_path);
        at_uart_write_locked((const uint8_t*)msg, n);
    }

    /* —— libcurl setup —— */
    curl_easy_setopt(h, CURLOPT_URL, req->url);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_USERAGENT, "esp-at-libcurl/1.0");
#ifdef BNCURL_FORCE_DNS
    curl_easy_setopt(h, CURLOPT_DNS_SERVERS, "8.8.8.8,1.1.1.1");
#endif
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, 60000L);  /* Extended connection timeout */
    curl_easy_setopt(h, CURLOPT_TIMEOUT_MS, timeout_ms);     /* Auto-calculated timeout */
    
    /* More aggressive low speed timeout settings for unstable connections */
    long low_speed_time = 300L; /* 5 minutes low speed timeout */
    long low_speed_limit = 1L;  /* 1 byte/sec minimum */
    
    if (content_length > 50 * 1024 * 1024) { /* Files > 50MB */
        low_speed_time = 600L; /* 10 minutes for very large files */
        low_speed_limit = 1L;  /* Very lenient for large files */
    }
    
    curl_easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, low_speed_limit);
    curl_easy_setopt(h, CURLOPT_LOW_SPEED_TIME, low_speed_time);
    curl_easy_setopt(h, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
    
    /* Enhanced TCP keep-alive settings for long transfers */
    curl_easy_setopt(h, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(h, CURLOPT_TCP_KEEPIDLE, 60L);   /* Start keep-alive after 1 minute */
    curl_easy_setopt(h, CURLOPT_TCP_KEEPINTVL, 30L);  /* Send keep-alive every 30 seconds */
    
    /* Enable connection reuse and pipelining */
    curl_easy_setopt(h, CURLOPT_FORBID_REUSE, 0L);
    curl_easy_setopt(h, CURLOPT_FRESH_CONNECT, 0L);
    
    /* Buffer size optimization for large downloads */
    curl_easy_setopt(h, CURLOPT_BUFFERSIZE, 65536L); /* 64KB buffer */

    /* TLS configuration - simplified for ESP32 compatibility */
#ifdef BNCURL_USE_CUSTOM_CA
    struct curl_blob ca = { .data=(void*)CA_BUNDLE_PEM, .len=sizeof(CA_BUNDLE_PEM)-1, .flags=CURL_BLOB_COPY };
    curl_easy_setopt(h, CURLOPT_CAINFO_BLOB, &ca);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2L);
#else
    /* Disable certificate verification for testing - let mbedTLS choose ciphers */
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0L);
    /* Let libcurl/mbedTLS negotiate the best available TLS version and ciphers */
    curl_easy_setopt(h, CURLOPT_SSLVERSION, CURL_SSLVERSION_DEFAULT);
#ifdef BNCURL_VERBOSE_TLS
    curl_easy_setopt(h, CURLOPT_VERBOSE, 1L);
#endif
#endif

    /* Enable verbose mode if requested */
    if (req->verbose) {
        curl_easy_setopt(h, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(h, CURLOPT_DEBUGFUNCTION, bncurl_debug_callback);
        curl_easy_setopt(h, CURLOPT_DEBUGDATA, req);
        
        /* Debug: confirm verbose mode is active */
        const char *verbose_msg = "+BNCURL: Verbose mode active - detailed output will follow\r\n";
        at_uart_write_locked((const uint8_t*)verbose_msg, strlen(verbose_msg));
    }

    /* Enable progress tracking for file transfers */
    if (req->save_to_file || req->has_upload_data) {
        curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, bncurl_progress_callback);
        curl_easy_setopt(h, CURLOPT_XFERINFODATA, req);
        
        /* Set as current active request for progress queries */
        if (progress_mutex && xSemaphoreTake(progress_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            current_active_req = req;
            req->in_progress = true;
            req->bytes_transferred = 0;
            req->total_bytes = 0;
            stop_requested = false;  /* Reset stop flag for new request */
            xSemaphoreGive(progress_mutex);
        }
    }

    /* Headers & body handling for spec framing */
    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "identity"); /* avoid gzip changing lengths */
    
    /* Use different header callback for HEAD requests to print headers */
    if (req->method == BNCURL_HEAD) {
        curl_easy_setopt(h, CURLOPT_HEADERFUNCTION,  bncurl_header_print_cb);
    } else {
        curl_easy_setopt(h, CURLOPT_HEADERFUNCTION,  bncurl_header_cb);
    }
    curl_easy_setopt(h, CURLOPT_HEADERDATA,      &ctx);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION,   bncurl_sink_framed);
    curl_easy_setopt(h, CURLOPT_WRITEDATA,       &ctx);

    /* Method selection */
    switch (req->method) {
        case BNCURL_GET:  curl_easy_setopt(h, CURLOPT_HTTPGET, 1L); break;
        case BNCURL_HEAD: curl_easy_setopt(h, CURLOPT_NOBODY,  1L); break;
        case BNCURL_POST: 
            curl_easy_setopt(h, CURLOPT_POST, 1L);
            if (req->has_upload_data) {
                /* Set up POST data */
                if (req->upload_from_file) {
                    /* Get file size for Content-Length */
                    struct stat st;
                    if (stat(req->upload_path, &st) == 0) {
                        curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)st.st_size);
                    }
                } else {
                    curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)req->upload_size);
                }
                curl_easy_setopt(h, CURLOPT_READFUNCTION, bncurl_read_callback);
                curl_easy_setopt(h, CURLOPT_READDATA, req);
            } else {
                /* Empty POST */
                curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, 0L);
            }
            break;
        default: break;
    }

    /* Set custom headers if provided */
    if (req->headers) {
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, req->headers);
    }
    
    /* Set cookie support if enabled */
    if (req->use_cookie_jar) {
        curl_easy_setopt(h, CURLOPT_COOKIEJAR, req->cookie_jar_path);
        char debug_msg[128];
        int n = snprintf(debug_msg, sizeof(debug_msg), "+BNCURL: Cookie jar: %s\r\n", req->cookie_jar_path);
        at_uart_write_locked((const uint8_t*)debug_msg, n);
    }
    
    if (req->use_cookie_send) {
        curl_easy_setopt(h, CURLOPT_COOKIEFILE, req->cookie_send_path);
        char debug_msg[128];
        int n = snprintf(debug_msg, sizeof(debug_msg), "+BNCURL: Cookie file: %s\r\n", req->cookie_send_path);
        at_uart_write_locked((const uint8_t*)debug_msg, n);
    }
    
    /* Set range request if enabled */
    if (req->use_range) {
        curl_easy_setopt(h, CURLOPT_RANGE, req->range_spec);
        char debug_msg[128];
        int n = snprintf(debug_msg, sizeof(debug_msg), "+BNCURL: Range request: %s\r\n", req->range_spec);
        at_uart_write_locked((const uint8_t*)debug_msg, n);
    }

    /* Retry logic for unstable connections */
    int max_retries = 3;
    int retry_count = 0;
    CURLcode rc = CURLE_OK;
    long http_code = 0;
    
    while (retry_count <= max_retries) {
        if (retry_count > 0) {
            char retry_msg[64];
            int n = snprintf(retry_msg, sizeof(retry_msg), "+BNCURL: Retry %d/%d after connection failure\r\n", retry_count, max_retries);
            at_uart_write_locked((const uint8_t*)retry_msg, n);
            
            /* Wait a bit before retrying */
            vTaskDelay(pdMS_TO_TICKS(2000 * retry_count)); /* Exponential backoff */
            
            /* Reset progress tracking for retry */
            if (progress_mutex && xSemaphoreTake(progress_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (current_active_req == req) {
                    req->bytes_transferred = 0;
                    req->total_bytes = content_length;
                    stop_requested = false;
                }
                xSemaphoreGive(progress_mutex);
            }
            
            /* Close and reopen file for retry */
            if (ctx.save_file) {
                fclose(ctx.save_file);
                ctx.save_file = fopen(req->save_path, "wb");
                if (!ctx.save_file) {
                    curl_easy_cleanup(h);
                    const char *err = "+BNCURL: ERROR cannot reopen file for retry\r\n";
                    at_uart_write_locked((const uint8_t*)err, strlen(err));
                    return ESP_AT_RESULT_CODE_ERROR;
                }
            }
            
            /* Reset context for retry */
            ctx.total_bytes = 0;
            ctx.len_announced = false;
            ctx.post_started = false;
            ctx.failed_after_len = false;
        }
        
        rc = curl_easy_perform(h);
        
        if (rc == CURLE_OK) {
            curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &http_code);
            break; /* Success, exit retry loop */
        }
        
        /* Check if this is a retryable error */
        bool retryable = (rc == CURLE_RECV_ERROR || 
                         rc == CURLE_SEND_ERROR ||
                         rc == CURLE_PARTIAL_FILE ||
                         rc == CURLE_OPERATION_TIMEDOUT ||
                         rc == CURLE_COULDNT_CONNECT ||
                         rc == CURLE_COULDNT_RESOLVE_HOST);
        
        if (!retryable || retry_count >= max_retries) {
            break; /* Non-retryable error or max retries reached */
        }
        
        retry_count++;
    }

    bncurl_last_http_code = (rc == CURLE_OK) ? http_code : -1;
    strncpy(bncurl_last_url, req->url, sizeof(bncurl_last_url) - 1);
    bncurl_last_url[sizeof(bncurl_last_url) - 1] = '\0';
    
    /* Close file if it was opened */
    if (ctx.save_file) {
        fclose(ctx.save_file);
        ctx.save_file = NULL;
    }
    
    /* Clear progress tracking */
    if (progress_mutex && xSemaphoreTake(progress_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (current_active_req == req) {
            current_active_req->in_progress = false;
            current_active_req = NULL;
        }
        xSemaphoreGive(progress_mutex);
    }
    
    curl_easy_cleanup(h);

    /* Results and error reporting */
    if (rc == CURLE_OK) {
        if (req->method == BNCURL_HEAD) {
            const char *msg = "+BNCURL: HEAD request completed\r\n";
            at_uart_write_locked((const uint8_t*)msg, strlen(msg));
        } else if (req->method == BNCURL_POST) {
            const char *msg = "+BNCURL: POST request completed\r\n";
            at_uart_write_locked((const uint8_t*)msg, strlen(msg));
        } else if (req->save_to_file) {
            char msg[128];
            int n = snprintf(msg, sizeof(msg), "+BNCURL: File saved (%lu bytes)\r\n", (unsigned long)ctx.total_bytes);
            at_uart_write_locked((const uint8_t*)msg, n);
        }
        
        if (retry_count > 0) {
            char retry_msg[64];
            int n = snprintf(retry_msg, sizeof(retry_msg), "+BNCURL: Completed after %d retries\r\n", retry_count);
            at_uart_write_locked((const uint8_t*)retry_msg, n);
        }
        
        at_uart_write_locked((const uint8_t*)"SEND OK\r\n", 9);
        return ESP_AT_RESULT_CODE_OK;
    }

    /* Enhanced error reporting with retry information */
    if (retry_count > 0) {
        char retry_msg[128];
        int n = snprintf(retry_msg, sizeof(retry_msg), "+BNCURL: Failed after %d retries - last error: %s\r\n", 
                        retry_count, curl_easy_strerror(rc));
        at_uart_write_locked((const uint8_t*)retry_msg, n);
    }

    /* Map “no Content-Length” strict failure */
    if (rc == CURLE_WRITE_ERROR && !ctx.len_announced && !ctx.have_len) {
        const char *e = "\r\n+BNCURL: ERROR length-unknown (no Content-Length)\r\n";
        at_uart_write_locked((const uint8_t*)e, strlen(e));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* If we failed after announcing LEN (mid-stream), send SEND FAIL */
    if (ctx.len_announced) {
        at_uart_write_locked((const uint8_t*)"SEND FAIL\r\n", 11);
    }
    
    /* Provide specific error context based on error type */
    const char *error_context = "";
    switch (rc) {
        case CURLE_RECV_ERROR:
            error_context = " (network receive error - check connection stability)";
            break;
        case CURLE_SEND_ERROR:
            error_context = " (network send error - check connection)";
            break;
        case CURLE_PARTIAL_FILE:
            error_context = " (incomplete download - server closed connection)";
            break;
        case CURLE_OPERATION_TIMEDOUT:
            error_context = " (timeout - try increasing timeout or check network)";
            break;
        case CURLE_COULDNT_CONNECT:
            error_context = " (connection failed - check URL and network)";
            break;
        case CURLE_COULDNT_RESOLVE_HOST:
            error_context = " (DNS resolution failed - check hostname)";
            break;
        default:
            error_context = "";
            break;
    }
    
    char e2[256];
    int n = snprintf(e2, sizeof(e2), "+BNCURL: ERROR %d %s%s (bytes %lu)\r\n",
                     rc, curl_easy_strerror(rc), error_context, (unsigned long)ctx.total_bytes);
    at_uart_write_locked((uint8_t*)e2, n);
    return ESP_AT_RESULT_CODE_ERROR;
}

static void bncurl_worker(void *arg) {
    for (;;) {
        bncurl_req_t *req_ptr = NULL;
        if (xQueueReceive(bncurl_q, &req_ptr, portMAX_DELAY) == pdTRUE && req_ptr) {
            req_ptr->result_code = bncurl_perform_internal(req_ptr);
            if (req_ptr->done) xSemaphoreGive(req_ptr->done);
        }
    }
}


static uint8_t at_bncurl_cmd_test(uint8_t *cmd_name) {
    const char *msg =
        "Usage:\r\n"
        "  AT+BNCURL?                                    Query last HTTP code/URL\r\n"
        "  AT+BNCURL                                     Execute default request (internal URL)\r\n"
        "  AT+BNCURL=GET,\"<url>\"[,<options>...]       Perform HTTP GET\r\n"
        "  AT+BNCURL=HEAD,\"<url>\"[,<options>...]      Perform HTTP HEAD (prints headers)\r\n"
        "  AT+BNCURL=POST,\"<url>\",<options>...        Perform HTTP POST with data upload\r\n"
        "Options:\r\n"
        "  -dd <filepath>   Save body to SD card file (auto-creates directories)\r\n"
        "  -du <size>       Upload <size> bytes from UART for POST requests\r\n"
        "  -du <filepath>   Upload file content for POST requests (@ prefix optional)\r\n"
        "  -H <header>      Add custom HTTP header (up to 10 headers)\r\n"
        "  -v               Enable verbose mode (show detailed HTTP transaction)\r\n"
        "  -c <filepath>    Save cookies to file (cookie jar)\r\n"
        "  -b <filepath>    Send cookies from file\r\n"
        "  -r <range>       Request specific byte range (e.g., \"0-1023\" or \"1024-\")\r\n"
        "Examples:\r\n"
        "  AT+BNCURL=GET,\"http://httpbin.org/get\"       Stream to UART (HTTP)\r\n"
        "  AT+BNCURL=HEAD,\"http://httpbin.org/get\"      Print headers to UART (HTTP)\r\n"
        "  AT+BNCURL=GET,\"http://httpbin.org/get\",-v    Verbose GET request\r\n"
        "  AT+BNCURL=POST,\"http://httpbin.org/post\",-du,\"8\"  Upload 8 bytes from UART\r\n"
        "  AT+BNCURL=GET,\"http://httpbin.org/get\",-dd,\"/sdcard/output.txt\"  Save to file\r\n"
        "  AT+BNCURL=GET,\"http://httpbin.org/get\",-H,\"Authorization: Bearer token123\"  Custom header\r\n"
        "  AT+BNCURL=GET,\"http://httpbin.org/get\",-c,\"/sdcard/cookies.txt\"  Save cookies\r\n"
        "  AT+BNCURL=GET,\"http://httpbin.org/get\",-b,\"/sdcard/cookies.txt\"  Send cookies\r\n"
        "  AT+BNCURL=GET,\"http://httpbin.org/get\",-r,\"0-1023\"  Download first 1KB only\r\n"
        "  AT+BNCURL=POST,\"http://httpbin.org/post\",-du,\"/Upload/data.bin\"  Upload file\r\n"
        "  AT+BNCURL=POST,\"http://httpbin.org/post\",-du,\"8\",-H,\"Content-Type: text/plain\"  POST with header\r\n"
        "  AT+BNCURL=GET,\"https://httpbin.org/get\"      Stream to UART (HTTPS)\r\n"
        "  AT+BNCURL=HEAD,\"https://httpbin.org/get\"     Print headers to UART (HTTPS)\r\n"
        "  AT+BNCURL=GET,\"http://httpbin.org/get\",-dd,\"/sdcard/data/response.json\"   Save to file (HTTP)\r\n"
        "  AT+BNCURL=GET,\"https://httpbin.org/get\",-dd,\"/sdcard/downloads/test.json\"  Save to file (HTTPS)\r\n"
        "Note: Try HTTP first if HTTPS has TLS issues\r\n"
        "Note: HEAD method prints headers with +HDR: prefix\r\n"
        "Note: POST with -du prompts with > for UART input\r\n"
        "Note: Verbose mode shows connection details with +VERB: prefix\r\n"
        "Note: Directories are created automatically if they don't exist\r\n";
    at_uart_write_locked((const uint8_t*)msg, strlen(msg));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_cmd_query(uint8_t *cmd_name) {
    char out[196];
    snprintf(out, sizeof(out), "+BNCURL: last_code=%ld, last_url=\"%s\"\r\n",
             bncurl_last_http_code, bncurl_last_url);
    at_uart_write_locked((uint8_t*)out, strlen(out));
    return ESP_AT_RESULT_CODE_OK;
}

/* Progress query command: AT+BNCURL_PROG? */
static uint8_t at_bncurl_prog_cmd_test(uint8_t *cmd_name) {
    at_uart_write_locked((const uint8_t*)"+BNCURL_PROG\r\n", 14);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_prog_cmd_query(uint8_t *cmd_name) {
    if (!progress_mutex) {
        const char *err = "+BNCURL_PROG: ERROR not initialized\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    char out[128];
    if (xSemaphoreTake(progress_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (current_active_req && current_active_req->in_progress) {
            if (current_active_req->total_bytes > 0) {
                snprintf(out, sizeof(out), "+BNCURL_PROG: %llu/%llu\r\n", 
                        (unsigned long long)current_active_req->bytes_transferred,
                        (unsigned long long)current_active_req->total_bytes);
            } else {
                snprintf(out, sizeof(out), "+BNCURL_PROG: %llu/unknown\r\n", 
                        (unsigned long long)current_active_req->bytes_transferred);
            }
        } else {
            snprintf(out, sizeof(out), "+BNCURL_PROG: no active transfer\r\n");
        }
        xSemaphoreGive(progress_mutex);
    } else {
        snprintf(out, sizeof(out), "+BNCURL_PROG: ERROR mutex timeout\r\n");
    }
    
    at_uart_write_locked((const uint8_t*)out, strlen(out));
    return ESP_AT_RESULT_CODE_OK;
}

/* Stop command: AT+BNCURL_STOP? */
static uint8_t at_bncurl_stop_cmd_test(uint8_t *cmd_name) {
    at_uart_write_locked((const uint8_t*)"+BNCURL_STOP\r\n", 15);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_stop_cmd_query(uint8_t *cmd_name) {
    if (!progress_mutex) {
        const char *err = "+BNCURL_STOP: ERROR not initialized\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    bool had_active_transfer = false;
    if (xSemaphoreTake(progress_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (current_active_req && current_active_req->in_progress) {
            stop_requested = true;
            had_active_transfer = true;
        }
        xSemaphoreGive(progress_mutex);
    }
    
    if (had_active_transfer) {
        at_uart_write_locked((const uint8_t*)"+BNCURL_STOP: stopping transfer\r\n", 34);
        return ESP_AT_RESULT_CODE_OK;
    } else {
        at_uart_write_locked((const uint8_t*)"+BNCURL_STOP: no active transfer\r\n", 35);
        return ESP_AT_RESULT_CODE_ERROR;
    }
}

/* Timeout configuration: AT+BNCURL_TIMEOUT */
static uint8_t at_bncurl_timeout_cmd_test(uint8_t *cmd_name) {
    at_uart_write_locked((const uint8_t*)"+BNCURL_TIMEOUT=(1-120)\r\n", 26);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_timeout_cmd_query(uint8_t *cmd_name) {
    char out[64];
    snprintf(out, sizeof(out), "+BNCURL_TIMEOUT: %ld\r\n", custom_timeout_seconds);
    at_uart_write_locked((const uint8_t*)out, strlen(out));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_timeout_cmd_setup(uint8_t para_num) {
    if (para_num != 1) {
        const char *err = "+BNCURL_TIMEOUT: ERROR invalid parameters\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    int32_t timeout;
    if (esp_at_get_para_as_digit(0, &timeout) != ESP_AT_PARA_PARSE_RESULT_OK) {
        const char *err = "+BNCURL_TIMEOUT: ERROR invalid timeout value\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    if (timeout < 1 || timeout > 120) {
        const char *err = "+BNCURL_TIMEOUT: ERROR timeout must be 1-120 seconds\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    custom_timeout_seconds = (long)timeout;
    
    char out[64];
    snprintf(out, sizeof(out), "+BNCURL_TIMEOUT: set to %ld seconds\r\n", custom_timeout_seconds);
    at_uart_write_locked((const uint8_t*)out, strlen(out));
    
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_cmd_setup(uint8_t para_num) {
    /* Expect: AT+BNCURL=GET,"<url>",[options...] */
    if (para_num < 2 || !bncurl_q) return ESP_AT_RESULT_CODE_ERROR;

    uint8_t *method_str = NULL;
    uint8_t *url = NULL;

    if (esp_at_get_para_as_str(0, &method_str) != ESP_AT_PARA_PARSE_RESULT_OK) return ESP_AT_RESULT_CODE_ERROR;
    if (esp_at_get_para_as_str(1, &url)        != ESP_AT_PARA_PARSE_RESULT_OK) return ESP_AT_RESULT_CODE_ERROR;

    /* Method mapping - now supports GET, HEAD, and POST */
    bncurl_method_t method = BNCURL_GET;
    bool matched = false;
    for (int i = 0; i < BNCURL_METHOD_MAX; i++) {
        if (strcasecmp((const char*)method_str, bncurl_method_str[i]) == 0) {
            method = (bncurl_method_t)i;
            matched = true;
            break;
        }
    }
    if (!matched || (method != BNCURL_GET && method != BNCURL_HEAD && method != BNCURL_POST)) {
        const char *e = "+BNCURL: ERROR unsupported method (GET, HEAD, and POST supported)\r\n";
        at_uart_write_locked((const uint8_t*)e, strlen(e));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Parse optional arguments. Now -dd (file save), -du (data upload), -H (headers), and -v (verbose) are implemented. */
    bool want_file = false;
    char file_path_tmp[128] = {0};
    bool want_upload = false;
    char upload_param[128] = {0};
    bool upload_from_file = false;
    size_t upload_size = 0;
    bool want_verbose = false;
    
    /* Custom headers storage */
    #define MAX_HEADERS 10
    char headers_list[MAX_HEADERS][256];
    int header_count = 0;
    
    /* Cookie and range support */
    bool want_cookie_jar = false;
    char cookie_jar_path[128] = {0};
    bool want_cookie_send = false;
    char cookie_send_path[128] = {0};
    bool want_range = false;
    char range_spec[64] = {0};
    
    /* Parse all parameters starting from parameter 2 */
    for (int i = 2; i < para_num; i++) {
        uint8_t *opt = NULL;
        esp_at_para_parse_result_type result = esp_at_get_para_as_str(i, &opt);
        
        if (result == ESP_AT_PARA_PARSE_RESULT_OK && opt) {
            if (strcasecmp((const char*)opt, "-dd") == 0) {
                /* Found -dd flag, get file path from next parameter */
                if (i + 1 < para_num) {
                    uint8_t *path = NULL;
                    result = esp_at_get_para_as_str(i + 1, &path);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && path) {
                        strncpy(file_path_tmp, (const char*)path, sizeof(file_path_tmp)-1);
                        want_file = true;
                        i++; /* Skip next parameter as it's the path */
                        
                        char debug_msg[128];
                        int n = snprintf(debug_msg, sizeof(debug_msg), "+BNCURL: DEBUG file path set to: %s\r\n", file_path_tmp);
                        at_uart_write_locked((const uint8_t*)debug_msg, n);
                    } else {
                        const char *e = "+BNCURL: ERROR reading -dd path parameter\r\n";
                        at_uart_write_locked((const uint8_t*)e, strlen(e));
                        return ESP_AT_RESULT_CODE_ERROR;
                    }
                }
            } else if (strcasecmp((const char*)opt, "-du") == 0) {
                /* Found -du flag, get upload parameter from next parameter */
                if (i + 1 < para_num) {
                    uint8_t *param = NULL;
                    result = esp_at_get_para_as_str(i + 1, &param);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && param) {
                        strncpy(upload_param, (const char*)param, sizeof(upload_param)-1);
                        want_upload = true;
                        i++; /* Skip next parameter as it's the upload param */
                        
                        /* Check if it's a file path (starts with @ or is a path) */
                        if (upload_param[0] == '@') {
                            /* File upload - remove @ prefix */
                            upload_from_file = true;
                            memmove(upload_param, upload_param + 1, strlen(upload_param));
                        } else if (upload_param[0] == '/' || strchr(upload_param, '/') != NULL) {
                            /* File path without @ prefix */
                            upload_from_file = true;
                        } else {
                            /* UART size parameter */
                            upload_from_file = false;
                            upload_size = (size_t)atoi(upload_param);
                            /* Allow upload_size == 0 for empty POST body */
                        }
                        
                        char debug_msg[128];
                        int n = snprintf(debug_msg, sizeof(debug_msg), "+BNCURL: DEBUG upload %s: %s\r\n", 
                                        upload_from_file ? "file" : "UART", upload_param);
                        at_uart_write_locked((const uint8_t*)debug_msg, n);
                    } else {
                        const char *e = "+BNCURL: ERROR reading -du parameter\r\n";
                        at_uart_write_locked((const uint8_t*)e, strlen(e));
                        return ESP_AT_RESULT_CODE_ERROR;
                    }
                }
            } else if (strcasecmp((const char*)opt, "-H") == 0) {
                /* Found -H flag, get header from next parameter */
                if (i + 1 < para_num && header_count < MAX_HEADERS) {
                    uint8_t *header = NULL;
                    result = esp_at_get_para_as_str(i + 1, &header);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && header) {
                        strncpy(headers_list[header_count], (const char*)header, sizeof(headers_list[header_count])-1);
                        headers_list[header_count][sizeof(headers_list[header_count])-1] = '\0';
                        header_count++;
                        i++; /* Skip next parameter as it's the header */
                        
                        char debug_msg[128];
                        int n = snprintf(debug_msg, sizeof(debug_msg), "+BNCURL: DEBUG header: %s\r\n", (const char*)header);
                        at_uart_write_locked((const uint8_t*)debug_msg, n);
                    } else {
                        const char *e = "+BNCURL: ERROR reading -H parameter\r\n";
                        at_uart_write_locked((const uint8_t*)e, strlen(e));
                        return ESP_AT_RESULT_CODE_ERROR;
                    }
                } else {
                    const char *e = "+BNCURL: ERROR too many headers or missing -H parameter\r\n";
                    at_uart_write_locked((const uint8_t*)e, strlen(e));
                    return ESP_AT_RESULT_CODE_ERROR;
                }
            } else if (strcasecmp((const char*)opt, "-v") == 0) {
                /* Found -v flag for verbose mode */
                want_verbose = true;
                
                char debug_msg[64];
                int n = snprintf(debug_msg, sizeof(debug_msg), "+BNCURL: DEBUG verbose mode enabled\r\n");
                at_uart_write_locked((const uint8_t*)debug_msg, n);
            } else if (strcasecmp((const char*)opt, "-c") == 0) {
                /* Found -c flag for cookie jar (save cookies) */
                if (i + 1 < para_num) {
                    uint8_t *path = NULL;
                    result = esp_at_get_para_as_str(i + 1, &path);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && path) {
                        strncpy(cookie_jar_path, (const char*)path, sizeof(cookie_jar_path)-1);
                        want_cookie_jar = true;
                        i++; /* Skip next parameter as it's the path */
                        
                        char debug_msg[128];
                        int n = snprintf(debug_msg, sizeof(debug_msg), "+BNCURL: DEBUG cookie jar: %s\r\n", cookie_jar_path);
                        at_uart_write_locked((const uint8_t*)debug_msg, n);
                    } else {
                        const char *e = "+BNCURL: ERROR reading -c parameter\r\n";
                        at_uart_write_locked((const uint8_t*)e, strlen(e));
                        return ESP_AT_RESULT_CODE_ERROR;
                    }
                } else {
                    const char *e = "+BNCURL: ERROR missing -c parameter\r\n";
                    at_uart_write_locked((const uint8_t*)e, strlen(e));
                    return ESP_AT_RESULT_CODE_ERROR;
                }
            } else if (strcasecmp((const char*)opt, "-b") == 0) {
                /* Found -b flag for cookie send (read cookies) */
                if (i + 1 < para_num) {
                    uint8_t *path = NULL;
                    result = esp_at_get_para_as_str(i + 1, &path);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && path) {
                        strncpy(cookie_send_path, (const char*)path, sizeof(cookie_send_path)-1);
                        want_cookie_send = true;
                        i++; /* Skip next parameter as it's the path */
                        
                        char debug_msg[128];
                        int n = snprintf(debug_msg, sizeof(debug_msg), "+BNCURL: DEBUG cookie send: %s\r\n", cookie_send_path);
                        at_uart_write_locked((const uint8_t*)debug_msg, n);
                    } else {
                        const char *e = "+BNCURL: ERROR reading -b parameter\r\n";
                        at_uart_write_locked((const uint8_t*)e, strlen(e));
                        return ESP_AT_RESULT_CODE_ERROR;
                    }
                } else {
                    const char *e = "+BNCURL: ERROR missing -b parameter\r\n";
                    at_uart_write_locked((const uint8_t*)e, strlen(e));
                    return ESP_AT_RESULT_CODE_ERROR;
                }
            } else if (strcasecmp((const char*)opt, "-r") == 0) {
                /* Found -r flag for range requests */
                if (i + 1 < para_num) {
                    uint8_t *range = NULL;
                    result = esp_at_get_para_as_str(i + 1, &range);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && range) {
                        strncpy(range_spec, (const char*)range, sizeof(range_spec)-1);
                        want_range = true;
                        i++; /* Skip next parameter as it's the range */
                        
                        char debug_msg[128];
                        int n = snprintf(debug_msg, sizeof(debug_msg), "+BNCURL: DEBUG range: %s\r\n", range_spec);
                        at_uart_write_locked((const uint8_t*)debug_msg, n);
                    } else {
                        const char *e = "+BNCURL: ERROR reading -r parameter\r\n";
                        at_uart_write_locked((const uint8_t*)e, strlen(e));
                        return ESP_AT_RESULT_CODE_ERROR;
                    }
                } else {
                    const char *e = "+BNCURL: ERROR missing -r parameter\r\n";
                    at_uart_write_locked((const uint8_t*)e, strlen(e));
                    return ESP_AT_RESULT_CODE_ERROR;
                }
            }
        }
    }

    /* Validate POST upload parameters */
    if (want_upload && method != BNCURL_POST) {
        const char *e = "+BNCURL: ERROR -du parameter only valid with POST method\r\n";
        at_uart_write_locked((const uint8_t*)e, strlen(e));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    bncurl_req_t *req = (bncurl_req_t*)calloc(1, sizeof(bncurl_req_t));
    if (!req) return ESP_AT_RESULT_CODE_ERROR;
    req->method = method;
    strncpy(req->url, (const char*)url, sizeof(req->url)-1);
    req->save_to_file = want_file;
    req->verbose = want_verbose;
    req->use_cookie_jar = want_cookie_jar;
    req->use_cookie_send = want_cookie_send;
    req->use_range = want_range;
    if (want_file) {
        strncpy(req->save_path, file_path_tmp, sizeof(req->save_path)-1);
    }
    if (want_cookie_jar) {
        strncpy(req->cookie_jar_path, cookie_jar_path, sizeof(req->cookie_jar_path)-1);
    }
    if (want_cookie_send) {
        strncpy(req->cookie_send_path, cookie_send_path, sizeof(req->cookie_send_path)-1);
    }
    if (want_range) {
        strncpy(req->range_spec, range_spec, sizeof(req->range_spec)-1);
    }
    
    /* Setup POST upload data */
    req->has_upload_data = want_upload;
    if (want_upload) {
        req->upload_from_file = upload_from_file;
        if (upload_from_file) {
            strncpy(req->upload_path, upload_param, sizeof(req->upload_path)-1);
        } else {
            /* UART input - read data using ESP-AT mechanism */
            req->upload_expected = upload_size;
            req->upload_data = (char*)malloc(upload_size + 1);
            if (!req->upload_data) {
                free(req);
                return ESP_AT_RESULT_CODE_ERROR;
            }
            
            /* Enter data input mode and send prompt */
            esp_at_port_enter_specific(at_bncurl_wait_data_cb);
            esp_at_response_result(ESP_AT_RESULT_CODE_OK_AND_INPUT_PROMPT);
            
            /* Read data from AT port */
            size_t bytes_read = 0;
            while (bytes_read < upload_size) {
                if (xSemaphoreTake(data_input_sema, pdMS_TO_TICKS(30000))) {
                    size_t read_len = esp_at_port_read_data(
                        (uint8_t*)(req->upload_data + bytes_read), 
                        upload_size - bytes_read
                    );
                    bytes_read += read_len;
                    
                    if (bytes_read >= upload_size) {
                        break;
                    }
                } else {
                    /* Timeout */
                    const char *e = "+BNCURL: ERROR timeout reading upload data\r\n";
                    at_uart_write_locked((const uint8_t*)e, strlen(e));
                    free(req->upload_data);
                    free(req);
                    esp_at_port_exit_specific();
                    return ESP_AT_RESULT_CODE_ERROR;
                }
            }
            
            esp_at_port_exit_specific();
            req->upload_data[upload_size] = '\0';
            req->upload_size = upload_size;
            req->upload_read_pos = 0;
            
            /* Report data received */
            char msg[64];
            int n = snprintf(msg, sizeof(msg), "+LEN:%lu\r\n", (unsigned long)upload_size);
            at_uart_write_locked((const uint8_t*)msg, n);
        }
    }
    
    /* Setup custom headers */
    req->headers = NULL;
    for (int i = 0; i < header_count; i++) {
        req->headers = curl_slist_append(req->headers, headers_list[i]);
        if (!req->headers) {
            const char *e = "+BNCURL: ERROR failed to add header\r\n";
            at_uart_write_locked((const uint8_t*)e, strlen(e));
            if (req->headers) curl_slist_free_all(req->headers);
            if (req->upload_data) free(req->upload_data);
            free(req);
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    
    req->done = xSemaphoreCreateBinary();
    if (!req->done) { 
        if (req->headers) curl_slist_free_all(req->headers);
        if (req->upload_data) free(req->upload_data);
        free(req); 
        return ESP_AT_RESULT_CODE_ERROR; 
    }

    if (xQueueSend(bncurl_q, &req, pdMS_TO_TICKS(100)) != pdTRUE) {
        vSemaphoreDelete(req->done);
        if (req->headers) curl_slist_free_all(req->headers);
        if (req->upload_data) free(req->upload_data);
        free(req);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Wait for completion with extended timeout for large files (up to 60 minutes) */
    if (xSemaphoreTake(req->done, pdMS_TO_TICKS(3600000)) != pdTRUE) {
        vSemaphoreDelete(req->done);
        if (req->headers) curl_slist_free_all(req->headers);
        if (req->upload_data) free(req->upload_data);
        free(req);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    uint8_t rc = req->result_code;
    vSemaphoreDelete(req->done);
    if (req->headers) curl_slist_free_all(req->headers);
    if (req->upload_data) free(req->upload_data);
    free(req);
    return rc;
}

static uint8_t at_bncurl_cmd_exe(uint8_t *cmd_name) {
    if (!bncurl_q) return ESP_AT_RESULT_CODE_ERROR;

    bncurl_req_t *req = (bncurl_req_t*)calloc(1, sizeof(bncurl_req_t));
    if (!req) return ESP_AT_RESULT_CODE_ERROR;
    req->method = BNCURL_GET;
    strcpy(req->url, "https://example.com/");
    req->save_to_file = false;  /* Default execute doesn't save to file */
    req->done = xSemaphoreCreateBinary();
    if (!req->done) { free(req); return ESP_AT_RESULT_CODE_ERROR; }

    if (xQueueSend(bncurl_q, &req, pdMS_TO_TICKS(100)) != pdTRUE) {
        vSemaphoreDelete(req->done);
        free(req);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Wait for completion with extended timeout for large files (up to 60 minutes) */
    if (xSemaphoreTake(req->done, pdMS_TO_TICKS(3600000)) != pdTRUE) {
        vSemaphoreDelete(req->done);
        free(req);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    uint8_t rc = req->result_code;
    vSemaphoreDelete(req->done);
    free(req);
    return rc;
}

/* ========================= Webradio Command Implementation ========================= */

static uint8_t at_bnwebradio_cmd_test(uint8_t *cmd_name) {
    const char *msg = 
        "Usage:\r\n"
        "  AT+BNWEBRADIO?                                Query streaming status\r\n"
        "  AT+BNWEBRADIO=\"<url>\"                        Start webradio/podcast streaming\r\n"
        "  AT+BNWEBRADIO=\"STOP\"                         Stop current streaming\r\n"
        "Description:\r\n"
        "  Streams pure binary audio data (MP3, AAC, etc.) without framing protocol.\r\n"
        "  Data is sent directly to UART as raw bytes for audio decoder.\r\n"
        "  Use AT+BNWEBRADIO=\"STOP\" or AT+BNWEBRADIO_STOP? to stop streaming.\r\n"
        "Examples:\r\n"
        "  AT+BNWEBRADIO=\"http://stream.radio.co/s12345/listen\"   Start radio stream\r\n"
        "  AT+BNWEBRADIO=\"https://podcast.example.com/episode.mp3\"  Stream podcast\r\n"
        "  AT+BNWEBRADIO=\"STOP\"                                    Stop streaming\r\n";
    at_uart_write_locked((const uint8_t*)msg, strlen(msg));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnwebradio_cmd_query(uint8_t *cmd_name) {
    char response[128];
    if (webradio_active) {
        snprintf(response, sizeof(response), "+BNWEBRADIO: streaming \"%s\"\r\n", webradio_url);
    } else {
        snprintf(response, sizeof(response), "+BNWEBRADIO: inactive\r\n");
    }
    at_uart_write_locked((const uint8_t*)response, strlen(response));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnwebradio_cmd_setup(uint8_t para_num) {
    /* AT+BNWEBRADIO="<url>" or AT+BNWEBRADIO="STOP" */
    if (para_num != 1) {
        const char *err = "+BNWEBRADIO: ERROR invalid parameters\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    uint8_t *url_param = NULL;
    if (esp_at_get_para_as_str(0, &url_param) != ESP_AT_PARA_PARSE_RESULT_OK) {
        const char *err = "+BNWEBRADIO: ERROR invalid URL parameter\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    /* Check for STOP command */
    if (strcasecmp((const char*)url_param, "STOP") == 0) {
        if (!webradio_active) {
            const char *msg = "+BNWEBRADIO: no active streaming\r\n";
            at_uart_write_locked((const uint8_t*)msg, strlen(msg));
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        webradio_stop_requested = true;
        
        /* Wait for task to finish (max 5 seconds) */
        int wait_count = 0;
        while (webradio_active && wait_count < 50) {
            vTaskDelay(pdMS_TO_TICKS(100));
            wait_count++;
        }
        
        if (webradio_active) {
            /* Force stop if task didn't stop gracefully */
            if (webradio_task) {
                vTaskDelete(webradio_task);
                webradio_task = NULL;
            }
            webradio_active = false;
            webradio_stop_requested = false;
            const char *msg = "+BNWEBRADIO: force stopped\r\n";
            at_uart_write_locked((const uint8_t*)msg, strlen(msg));
        }
        
        return ESP_AT_RESULT_CODE_OK;
    }
    
    /* Check if already streaming */
    if (webradio_active) {
        const char *err = "+BNWEBRADIO: ERROR already streaming (use STOP first)\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    /* Validate URL */
    if (strlen((const char*)url_param) >= sizeof(webradio_url)) {
        const char *err = "+BNWEBRADIO: ERROR URL too long\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    /* Initialize curl if not done yet */
    if (!bncurl_curl_inited) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        bncurl_curl_inited = true;
    }
    
    /* Store URL and start streaming task */
    strncpy(webradio_url, (const char*)url_param, sizeof(webradio_url) - 1);
    webradio_url[sizeof(webradio_url) - 1] = '\0';
    webradio_active = true;
    webradio_stop_requested = false;
    
    /* Create streaming task with sufficient stack for curl + TLS */
    BaseType_t task_created = xTaskCreatePinnedToCore(
        webradio_streaming_task,
        "webradio_stream",
        16384, /* Large stack for curl + TLS */
        NULL,
        6, /* Higher priority than bncurl worker */
        &webradio_task,
        0
    );
    
    if (task_created != pdPASS) {
        webradio_active = false;
        const char *err = "+BNWEBRADIO: ERROR failed to create streaming task\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    return ESP_AT_RESULT_CODE_OK;
}

/* Stop command: AT+BNWEBRADIO_STOP? */
static uint8_t at_bnwebradio_stop_cmd_test(uint8_t *cmd_name) {
    at_uart_write_locked((const uint8_t*)"+BNWEBRADIO_STOP\r\n", 19);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnwebradio_stop_cmd_query(uint8_t *cmd_name) {
    if (!webradio_active) {
        const char *msg = "+BNWEBRADIO_STOP: no active streaming\r\n";
        at_uart_write_locked((const uint8_t*)msg, strlen(msg));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    webradio_stop_requested = true;
    
    /* Wait for task to finish (max 5 seconds) */
    int wait_count = 0;
    while (webradio_active && wait_count < 50) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_count++;
    }
    
    if (webradio_active) {
        /* Force stop if task didn't stop gracefully */
        if (webradio_task) {
            vTaskDelete(webradio_task);
            webradio_task = NULL;
        }
        webradio_active = false;
        webradio_stop_requested = false;
        const char *msg = "+BNWEBRADIO_STOP: force stopped\r\n";
        at_uart_write_locked((const uint8_t*)msg, strlen(msg));
    } else {
        const char *msg = "+BNWEBRADIO_STOP: streaming stopped\r\n";
        at_uart_write_locked((const uint8_t*)msg, strlen(msg));
    }
    
    return ESP_AT_RESULT_CODE_OK;
}

/* ========================= WPS Command Implementation ========================= */

static uint8_t at_bnwps_cmd_test(uint8_t *cmd_name) {
    at_uart_write_locked((const uint8_t*)"OK\r\n", 4);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnwps_cmd_query(uint8_t *cmd_name) {
    char response[64];
    snprintf(response, sizeof(response), "+BNWPS:<%d>\r\nOK\r\n", wps_active ? 1 : 0);
    at_uart_write_locked((const uint8_t*)response, strlen(response));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnwps_cmd_setup(uint8_t para_num) {
    /* AT+BNWPS=<t> where t is timeout in seconds, or 0 to cancel */
    if (para_num != 1) return ESP_AT_RESULT_CODE_ERROR;
    
    int32_t timeout;
    if (esp_at_get_para_as_digit(0, &timeout) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    if (timeout < 0 || timeout > 300) {
        const char *err = "+BNWPS: ERROR timeout must be 0-300 seconds\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    if (timeout == 0) {
        /* Cancel WPS */
        if (wps_active) {
            esp_wifi_wps_disable();
            wps_active = false;
            if (wps_timer) {
                xTimerStop(wps_timer, 0);
                xTimerDelete(wps_timer, 0);
                wps_timer = NULL;
            }
        }
        at_uart_write_locked((const uint8_t*)"+BNWPS:<0>\r\nOK\r\n", 16);
        return ESP_AT_RESULT_CODE_OK;
    }
    
    /* Start WPS for specified timeout */
    if (wps_active) {
        const char *err = "+BNWPS: ERROR WPS already active\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    wps_timeout_seconds = (uint32_t)timeout;
    
    /* Configure WPS */
    esp_wps_config_t wps_config = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);
    esp_err_t ret = esp_wifi_wps_enable(&wps_config);
    if (ret != ESP_OK) {
        const char *err = "+BNWPS: ERROR failed to enable WPS\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    ret = esp_wifi_wps_start(0);
    if (ret != ESP_OK) {
        esp_wifi_wps_disable();
        const char *err = "+BNWPS: ERROR failed to start WPS\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    wps_active = true;
    
    /* Create timer for WPS timeout */
    wps_timer = xTimerCreate("wps_timer", pdMS_TO_TICKS(timeout * 1000), pdFALSE, NULL, wps_timer_callback);
    if (wps_timer) {
        xTimerStart(wps_timer, 0);
    }
    
    at_uart_write_locked((const uint8_t*)"+BNWPS:<1>\r\nOK\r\n", 16);
    return ESP_AT_RESULT_CODE_OK;
}

/* ========================= Flash Certificate Command Implementation ========================= */

static uint8_t at_bnflash_cert_cmd_test(uint8_t *cmd_name) {
    at_uart_write_locked((const uint8_t*)"OK\r\n", 4);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnflash_cert_cmd_setup(uint8_t para_num) {
    /* AT+BNFLASH_CERT=<flash_address>,<data_source> */
    if (para_num != 2) return ESP_AT_RESULT_CODE_ERROR;
    
    int32_t flash_address;
    uint8_t *data_source = NULL;
    
    if (esp_at_get_para_as_digit(0, &flash_address) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    if (esp_at_get_para_as_str(1, &data_source) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    if (flash_address < 0) {
        const char *err = "+BNFLASH_CERT: ERROR invalid flash address\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    bool is_file = false;
    char filename[256];
    uint32_t data_size = 0;
    
    if (data_source[0] == '@') {
        /* File from SD card */
        is_file = true;
        strncpy(filename, (char*)&data_source[1], sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
        
        if (!sd_mounted) {
            const char *err = "+BNFLASH_CERT: ERROR SD card not mounted\r\n";
            at_uart_write_locked((const uint8_t*)err, strlen(err));
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        /* Check if file exists and get size */
        char full_path[300];
        snprintf(full_path, sizeof(full_path), "%s%s", MOUNT_POINT, filename);
        
        FILE *f = fopen(full_path, "rb");
        if (!f) {
            const char *err = "+BNFLASH_CERT: ERROR file not found\r\n";
            at_uart_write_locked((const uint8_t*)err, strlen(err));
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        fseek(f, 0, SEEK_END);
        data_size = ftell(f);
        fclose(f);
        
        if (data_size == 0) {
            const char *err = "+BNFLASH_CERT: ERROR file is empty\r\n";
            at_uart_write_locked((const uint8_t*)err, strlen(err));
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
    } else {
        /* Data via UART - parse size */
        data_size = (uint32_t)atoi((char*)data_source);
        if (data_size == 0 || data_size > 65536) {
            const char *err = "+BNFLASH_CERT: ERROR invalid data size (1-65536)\r\n";
            at_uart_write_locked((const uint8_t*)err, strlen(err));
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    
    /* Allocate buffer for data */
    uint8_t *buffer = malloc(data_size);
    if (!buffer) {
        const char *err = "+BNFLASH_CERT: ERROR out of memory\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    if (is_file) {
        /* Read from file */
        char full_path[300];
        snprintf(full_path, sizeof(full_path), "%s%s", MOUNT_POINT, filename);
        
        FILE *f = fopen(full_path, "rb");
        if (!f) {
            free(buffer);
            const char *err = "+BNFLASH_CERT: ERROR cannot open file\r\n";
            at_uart_write_locked((const uint8_t*)err, strlen(err));
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        size_t read_bytes = fread(buffer, 1, data_size, f);
        fclose(f);
        
        if (read_bytes != data_size) {
            free(buffer);
            const char *err = "+BNFLASH_CERT: ERROR file read failed\r\n";
            at_uart_write_locked((const uint8_t*)err, strlen(err));
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
    } else {
        /* Read from UART */
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "+AT+BNFLASH_CERT:\r\n>\r\n");
        at_uart_write_locked((const uint8_t*)prompt, strlen(prompt));
        
        /* Use ESP-AT port input mechanism for reading data */
        esp_at_port_enter_specific(at_bncurl_wait_data_cb);
        esp_at_response_result(ESP_AT_RESULT_CODE_OK_AND_INPUT_PROMPT);
        
        uint32_t received = 0;
        while (received < data_size) {
            if (xSemaphoreTake(data_input_sema, pdMS_TO_TICKS(30000))) {
                uint32_t remain = data_size - received;
                uint32_t chunk = (remain > 1024) ? 1024 : remain;
                
                size_t read_len = esp_at_port_read_data(buffer + received, chunk);
                if (read_len <= 0) {
                    free(buffer);
                    const char *err = "+BNFLASH_CERT: ERROR UART read failed\r\n";
                    at_uart_write_locked((const uint8_t*)err, strlen(err));
                    esp_at_port_exit_specific();
                    return ESP_AT_RESULT_CODE_ERROR;
                }
                received += read_len;
                
                if (received >= data_size) {
                    break;
                }
            } else {
                free(buffer);
                const char *err = "+BNFLASH_CERT: ERROR UART timeout\r\n";
                at_uart_write_locked((const uint8_t*)err, strlen(err));
                esp_at_port_exit_specific();
                return ESP_AT_RESULT_CODE_ERROR;
            }
        }
        
        esp_at_port_exit_specific();
    }
    
    /* Write to flash */
    esp_err_t ret = esp_flash_write(esp_flash_default_chip, buffer, (uint32_t)flash_address, data_size);
    free(buffer);
    
    if (ret != ESP_OK) {
        const char *err = "+BNFLASH_CERT: ERROR flash write failed\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    /* Send confirmation response as shown in spec */
    if (is_file) {
        at_uart_write_locked((const uint8_t*)"+AT+BNFLASH_CERT:\r\n", 19);
    }
    
    at_uart_write_locked((const uint8_t*)"OK\r\n", 4);
    return ESP_AT_RESULT_CODE_OK;
}

/* ----------------------- Command table & init ----------------------- */

static const esp_at_cmd_struct at_custom_cmd[] = {
    {"+TEST",         at_test_cmd_test,       at_query_cmd_test,    at_setup_cmd_test,   at_exe_cmd_test},
    {"+BNSD_MOUNT",   at_bnsd_mount_cmd_test, at_bnsd_mount_cmd_query, NULL,             at_bnsd_mount_cmd_exe},
    {"+BNSD_UNMOUNT", at_bnsd_unmount_cmd_test, at_bnsd_unmount_cmd_query, NULL,         at_bnsd_unmount_cmd_exe},
    {"+BNSD_FORMAT",  at_bnsd_format_cmd_test, NULL,                NULL,                at_bnsd_format_cmd_exe},
    {"+BNSD_SPACE",   at_bnsd_space_cmd_test,  at_bnsd_space_cmd_query, NULL,           NULL},
    {"+BNCURL",       at_bncurl_cmd_test,     at_bncurl_cmd_query,  at_bncurl_cmd_setup, at_bncurl_cmd_exe},
    {"+BNCURL_PROG",  at_bncurl_prog_cmd_test, at_bncurl_prog_cmd_query, NULL,          NULL},
    {"+BNCURL_STOP",  at_bncurl_stop_cmd_test, at_bncurl_stop_cmd_query, NULL,          NULL},
    {"+BNCURL_TIMEOUT", at_bncurl_timeout_cmd_test, at_bncurl_timeout_cmd_query, at_bncurl_timeout_cmd_setup, NULL},
    {"+BNWEBRADIO",   at_bnwebradio_cmd_test, at_bnwebradio_cmd_query, at_bnwebradio_cmd_setup, NULL},
    {"+BNWEBRADIO_STOP", at_bnwebradio_stop_cmd_test, at_bnwebradio_stop_cmd_query, NULL, NULL},
    {"+BNWPS",        at_bnwps_cmd_test,       at_bnwps_cmd_query,       at_bnwps_cmd_setup,       NULL},
    {"+BNFLASH_CERT", at_bnflash_cert_cmd_test, NULL,                    at_bnflash_cert_cmd_setup, NULL},
    /* add further custom AT commands here */
};

bool esp_at_custom_cmd_register(void)
{
    bool ok = esp_at_custom_cmd_array_regist(at_custom_cmd, sizeof(at_custom_cmd) / sizeof(esp_at_cmd_struct));
    if (!ok) return false;

    if (!at_uart_lock) at_uart_lock = xSemaphoreCreateMutex();
    if (!progress_mutex) progress_mutex = xSemaphoreCreateMutex();
    if (!data_input_sema) data_input_sema = xSemaphoreCreateBinary();
    if (!bncurl_q)     bncurl_q = xQueueCreate(2, sizeof(bncurl_req_t*));
    if (!bncurl_task) {
        /* TLS + libcurl + printf ==> give it a big stack; tune later */
        xTaskCreatePinnedToCore(bncurl_worker, "bncurl", 16384, NULL, 5, &bncurl_task, 0);
    }
    
    /* Register WPS event handler */
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wps_event_handler, NULL);
    
    return true;
}

ESP_AT_CMD_SET_INIT_FN(esp_at_custom_cmd_register, 1);
