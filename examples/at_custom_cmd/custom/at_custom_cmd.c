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
#include "driver/sdspi_host.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include <curl/curl.h>
#include "esp_crt_bundle.h"

/* ========================= SD Card bits (unchanged) ========================= */

#define MOUNT_POINT "/sdcard"


#define PIN_NUM_CS    16
#define PIN_NUM_MOSI  17
#define PIN_NUM_CLK   21
#define PIN_NUM_MISO  20

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

/* Worker request object:
   - done: semaphore the AT handler waits on (so command is user-visible blocking)
   - result_code: set by worker to OK/ERROR
*/
typedef struct {
    bncurl_method_t method;
    char url[256];
    char save_path[256];         /* file path for -dd option */
    bool save_to_file;           /* true if saving to file */
    SemaphoreHandle_t done;
    uint8_t result_code;
} bncurl_req_t;


static QueueHandle_t     bncurl_q      = NULL;
static TaskHandle_t      bncurl_task   = NULL;
static SemaphoreHandle_t at_uart_lock  = NULL;

/* Thread-safe write to AT UART */
static inline void at_uart_write_locked(const uint8_t *data, size_t len) {
    if (at_uart_lock) xSemaphoreTake(at_uart_lock, portMAX_DELAY);
    esp_at_port_write_data(data, len);
    if (at_uart_lock) xSemaphoreGive(at_uart_lock);
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


static uint8_t bncurl_perform_internal(bncurl_method_t method, const char *url, const char *save_path, bool save_to_file) {
    if (!bncurl_curl_inited) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        bncurl_curl_inited = true;
    }
    CURL *h = curl_easy_init();
    if (!h) {
        const char *err = "+BNCURL: init failed\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    bncurl_ctx_t ctx = {0};
    ctx.save_to_file = save_to_file;
    
    /* Open file if saving to SD card */
    if (save_to_file && save_path) {
        /* Check if SD card is mounted */
        if (!sd_mounted) {
            const char *err = "+BNCURL: ERROR SD card not mounted\r\n";
            at_uart_write_locked((const uint8_t*)err, strlen(err));
            curl_easy_cleanup(h);
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        ctx.save_file = fopen(save_path, "wb");
        if (!ctx.save_file) {
            const char *err = "+BNCURL: ERROR cannot open file for writing\r\n";
            at_uart_write_locked((const uint8_t*)err, strlen(err));
            curl_easy_cleanup(h);
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        char msg[128];
        int n = snprintf(msg, sizeof(msg), "+BNCURL: Saving to file: %s\r\n", save_path);
        at_uart_write_locked((const uint8_t*)msg, n);
    }

    /* —— libcurl setup —— */
    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_USERAGENT, "esp-at-libcurl/1.0");
#ifdef BNCURL_FORCE_DNS
    curl_easy_setopt(h, CURLOPT_DNS_SERVERS, "8.8.8.8,1.1.1.1");
#endif
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, 30000L);  /* Increased timeout */
    curl_easy_setopt(h, CURLOPT_TIMEOUT_MS,        120000L); /* Increased timeout */
    curl_easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(h, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(h, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(h, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(h, CURLOPT_TCP_KEEPIDLE, 120L);
    curl_easy_setopt(h, CURLOPT_TCP_KEEPINTVL, 60L);

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

    /* Headers & body handling for spec framing */
    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "identity"); /* avoid gzip changing lengths */
    curl_easy_setopt(h, CURLOPT_HEADERFUNCTION,  bncurl_header_cb);
    curl_easy_setopt(h, CURLOPT_HEADERDATA,      &ctx);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION,   bncurl_sink_framed);
    curl_easy_setopt(h, CURLOPT_WRITEDATA,       &ctx);

    /* Method selection */
    switch (method) {
        case BNCURL_GET:  curl_easy_setopt(h, CURLOPT_HTTPGET, 1L); break;
        case BNCURL_HEAD: curl_easy_setopt(h, CURLOPT_NOBODY,  1L); break;
        case BNCURL_POST: curl_easy_setopt(h, CURLOPT_POST,    1L); /* add POSTFIELDS later */ break;
        default: break;
    }

    CURLcode rc = curl_easy_perform(h);
    long http_code = 0;
    if (rc == CURLE_OK) curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &http_code);

    bncurl_last_http_code = (rc == CURLE_OK) ? http_code : -1;
    strncpy(bncurl_last_url, url, sizeof(bncurl_last_url) - 1);
    bncurl_last_url[sizeof(bncurl_last_url) - 1] = '\0';
    
    /* Close file if it was opened */
    if (ctx.save_file) {
        fclose(ctx.save_file);
        ctx.save_file = NULL;
    }
    
    curl_easy_cleanup(h);

    /* Results and error reporting */
    if (rc == CURLE_OK) {
        if (save_to_file) {
            char msg[128];
            int n = snprintf(msg, sizeof(msg), "+BNCURL: File saved (%lu bytes)\r\n", (unsigned long)ctx.total_bytes);
            at_uart_write_locked((const uint8_t*)msg, n);
        }
        at_uart_write_locked((const uint8_t*)"SEND OK\r\n", 9);
        return ESP_AT_RESULT_CODE_OK;
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
    char e2[128];
    int n = snprintf(e2, sizeof(e2), "+BNCURL: ERROR %d %s (bytes %lu)\r\n",
                     rc, curl_easy_strerror(rc), (unsigned long)ctx.total_bytes);
    at_uart_write_locked((uint8_t*)e2, n);
    return ESP_AT_RESULT_CODE_ERROR;
}

static void bncurl_worker(void *arg) {
    for (;;) {
        bncurl_req_t *req_ptr = NULL;
        if (xQueueReceive(bncurl_q, &req_ptr, portMAX_DELAY) == pdTRUE && req_ptr) {
            req_ptr->result_code = bncurl_perform_internal(req_ptr->method, req_ptr->url, 
                                                          req_ptr->save_to_file ? req_ptr->save_path : NULL, 
                                                          req_ptr->save_to_file);
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
        "Options:\r\n"
        "  -dd <filepath>   Save body to SD card file (requires mounted SD)\r\n"
        "Examples:\r\n"
        "  AT+BNCURL=GET,\"http://httpbin.org/get\"       Stream to UART (HTTP)\r\n"
        "  AT+BNCURL=GET,\"https://httpbin.org/get\"      Stream to UART (HTTPS)\r\n"
        "  AT+BNCURL=GET,\"http://httpbin.org/get\",-dd,\"/sdcard/response.json\"   Save to file (HTTP)\r\n"
        "  AT+BNCURL=GET,\"https://httpbin.org/get\",-dd,\"/sdcard/response.json\"  Save to file (HTTPS)\r\n"
        "Note: Try HTTP first if HTTPS has TLS issues\r\n";
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

static uint8_t at_bncurl_cmd_setup(uint8_t para_num) {
    /* Expect: AT+BNCURL=GET,"<url>",[options...] */
    if (para_num < 2 || !bncurl_q) return ESP_AT_RESULT_CODE_ERROR;

    uint8_t *method_str = NULL;
    uint8_t *url = NULL;

    if (esp_at_get_para_as_str(0, &method_str) != ESP_AT_PARA_PARSE_RESULT_OK) return ESP_AT_RESULT_CODE_ERROR;
    if (esp_at_get_para_as_str(1, &url)        != ESP_AT_PARA_PARSE_RESULT_OK) return ESP_AT_RESULT_CODE_ERROR;

    /* Method mapping (currently only GET supported) */
    bncurl_method_t method = BNCURL_GET;
    bool matched = false;
    for (int i = 0; i < BNCURL_METHOD_MAX; i++) {
        if (strcasecmp((const char*)method_str, bncurl_method_str[i]) == 0) {
            method = (bncurl_method_t)i;
            matched = true;
            break;
        }
    }
    if (!matched || method != BNCURL_GET) {
        const char *e = "+BNCURL: ERROR unsupported method (only GET for now)\r\n";
        at_uart_write_locked((const uint8_t*)e, strlen(e));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Parse optional arguments. Now -dd (file save) is implemented. */
    bool want_file = false;
    char file_path_tmp[128] = {0};
    
    /* Check if we have at least 4 parameters for -dd option */
    if (para_num >= 4) {
        /* Try to parse parameter 2 as the -dd flag */
        uint8_t *opt = NULL;
        esp_at_para_parse_result_type result = esp_at_get_para_as_str(2, &opt);
        
        if (result == ESP_AT_PARA_PARSE_RESULT_OK && opt && strcasecmp((const char*)opt, "-dd") == 0) {
            /* Found -dd flag, now get the file path from parameter 3 */
            uint8_t *path = NULL;
            result = esp_at_get_para_as_str(3, &path);
            
            if (result == ESP_AT_PARA_PARSE_RESULT_OK && path) {
                strncpy(file_path_tmp, (const char*)path, sizeof(file_path_tmp)-1);
                want_file = true;
                
                /* Debug: confirm file path received */
                char debug_msg[128];
                int n = snprintf(debug_msg, sizeof(debug_msg), "+BNCURL: DEBUG file path set to: %s\r\n", file_path_tmp);
                at_uart_write_locked((const uint8_t*)debug_msg, n);
            } else {
                const char *e = "+BNCURL: ERROR reading -dd path parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
        } else {
            /* Debug: show parsing issue */
            char debug_msg[128];
            int n = snprintf(debug_msg, sizeof(debug_msg), "+BNCURL: DEBUG param 2 not -dd flag (result=%d)\r\n", result);
            at_uart_write_locked((const uint8_t*)debug_msg, n);
        }
    }

    bncurl_req_t *req = (bncurl_req_t*)calloc(1, sizeof(bncurl_req_t));
    if (!req) return ESP_AT_RESULT_CODE_ERROR;
    req->method = method;
    strncpy(req->url, (const char*)url, sizeof(req->url)-1);
    req->save_to_file = want_file;
    if (want_file) {
        strncpy(req->save_path, file_path_tmp, sizeof(req->save_path)-1);
    }
    req->done = xSemaphoreCreateBinary();
    if (!req->done) { free(req); return ESP_AT_RESULT_CODE_ERROR; }

    if (xQueueSend(bncurl_q, &req, pdMS_TO_TICKS(100)) != pdTRUE) {
        vSemaphoreDelete(req->done);
        free(req);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (xSemaphoreTake(req->done, pdMS_TO_TICKS(120000)) != pdTRUE) {
        vSemaphoreDelete(req->done);
        free(req);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    uint8_t rc = req->result_code;
    vSemaphoreDelete(req->done);
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

    if (xSemaphoreTake(req->done, pdMS_TO_TICKS(120000)) != pdTRUE) {
        vSemaphoreDelete(req->done);
        free(req);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    uint8_t rc = req->result_code;
    vSemaphoreDelete(req->done);
    free(req);
    return rc;
}

/* ----------------------- Command table & init ----------------------- */

static const esp_at_cmd_struct at_custom_cmd[] = {
    {"+TEST",         at_test_cmd_test,       at_query_cmd_test,    at_setup_cmd_test,   at_exe_cmd_test},
    {"+BNSD_MOUNT",   at_bnsd_mount_cmd_test, at_bnsd_mount_cmd_query, NULL,             at_bnsd_mount_cmd_exe},
    {"+BNSD_UNMOUNT", at_bnsd_unmount_cmd_test, at_bnsd_unmount_cmd_query, NULL,         at_bnsd_unmount_cmd_exe},
    {"+BNCURL",       at_bncurl_cmd_test,     at_bncurl_cmd_query,  at_bncurl_cmd_setup, at_bncurl_cmd_exe},
    /* add further custom AT commands here */
};

bool esp_at_custom_cmd_register(void)
{
    bool ok = esp_at_custom_cmd_array_regist(at_custom_cmd, sizeof(at_custom_cmd) / sizeof(esp_at_cmd_struct));
    if (!ok) return false;

    if (!at_uart_lock) at_uart_lock = xSemaphoreCreateMutex();
    if (!bncurl_q)     bncurl_q = xQueueCreate(2, sizeof(bncurl_req_t*));
    if (!bncurl_task) {
        /* TLS + libcurl + printf ==> give it a big stack; tune later */
        xTaskCreatePinnedToCore(bncurl_worker, "bncurl", 16384, NULL, 5, &bncurl_task, 0);
    }
    return true;
}

ESP_AT_CMD_SET_INIT_FN(esp_at_custom_cmd_register, 1);
