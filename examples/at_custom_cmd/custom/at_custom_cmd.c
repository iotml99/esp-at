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
#include "atbn_config.h"
#include "sd_card.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include <curl/curl.h>
#include "esp_crt_bundle.h"

static const char *TAG = "at_curl";

/* Global timeout setting for BNCURL operations (in seconds) */
static uint32_t bncurl_timeout_seconds = BNCURL_TIMEOUT_DEFAULT_SECONDS; /* Default 30 seconds */

/* Global variables for operation control */
static bool      bncurl_operation_running = false;  /* Track if operation is active */
static bool      bncurl_stop_requested = false;     /* Signal to stop current operation */


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


/* ================= HTTP method & framing config ================= */



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
    
    SemaphoreHandle_t done;
    uint8_t result_code;
} bncurl_req_t;


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

/* ================= Progress callback for stop detection ================= */
static int bncurl_progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)clientp; /* Unused */
    (void)dltotal; /* Unused */
    (void)dlnow;   /* Unused */
    (void)ultotal; /* Unused */
    (void)ulnow;   /* Unused */
    
    /* Check if stop was requested */
    if (bncurl_stop_requested) {
        ESP_LOGI(TAG, "BNCURL operation stopped by user request");
        return 1; /* Non-zero return value aborts the transfer */
    }
    
    return 0; /* Continue transfer */
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
        size_t chunk = remaining > BNCURL_UART_CHUNK_SIZE ? BNCURL_UART_CHUNK_SIZE : remaining;

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
        return 60000L; /* 1 minute for unknown size */
    }
    
    /* Calculate timeout for large files only (>10MB) */
    /* Assume minimum speed of 50KB/s (400 Kbps) for very slow connections */
    /* Add safety margin for connection setup, TLS handshake, and network fluctuations */
    
    /* Calculate timeout with safety margin */
    long calculated_timeout = BNCURL_BASE_TIMEOUT_MS + (content_length * 1000 * BNCURL_TIMEOUT_SAFETY_MARGIN / BNCURL_MIN_SPEED_BYTES_PER_SEC);
    
    /* Clamp to reasonable bounds */
    if (calculated_timeout < 60000L) calculated_timeout = 60000L; /* Min 1 minute for large files */
    if (calculated_timeout > BNCURL_MAX_TIMEOUT_MS) calculated_timeout = BNCURL_MAX_TIMEOUT_MS;
    
    /* Log timeout calculation for debugging */
    char debug_msg[BNCURL_DEBUG_MSG_SIZE];
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
    
    /* Temporarily reduce mbedTLS log level to suppress harmless timeout errors */
    esp_log_level_t old_mbedtls_level = esp_log_level_get("mbedtls");
    esp_log_level_t old_dynamic_impl_level = esp_log_level_get("Dynamic Impl");
    if (!req->verbose) {
        esp_log_level_set("mbedtls", ESP_LOG_WARN);
        esp_log_level_set("Dynamic Impl", ESP_LOG_WARN);
    }
    
    /* Get content length for timeout calculation (only for GET requests) */
    uint64_t content_length = 0;
    long timeout_ms = bncurl_timeout_seconds * 1000L; /* Use user-configurable timeout */
    
    if (req->method == BNCURL_GET) {
        content_length = get_content_length(req->url);
        /* For GET requests, only extend timeout if content is very large and user timeout is insufficient */
        /* Calculate minimum time needed for large files (>10MB) at slow speeds */
        if (content_length > (10 * 1024 * 1024)) { /* Files larger than 10MB */
            long calculated_timeout = calculate_timeout_ms(content_length);
            if (calculated_timeout > timeout_ms) {
                timeout_ms = calculated_timeout;
                ESP_LOGI(TAG, "Extended timeout to %ld ms for large file (%llu bytes)", timeout_ms, content_length);
            }
        }
        /* For small files, always respect user timeout */
    } else if (req->method == BNCURL_HEAD) {
        /* HEAD requests use user timeout (typically faster than user setting) */
        /* But ensure minimum reasonable timeout for HEAD */
        if (timeout_ms < 5000L) timeout_ms = 5000L; /* Min 5 seconds for HEAD */
    } else if (req->method == BNCURL_POST) {
        /* POST requests use user timeout, but ensure reasonable minimum */
        if (timeout_ms < 10000L) timeout_ms = 10000L; /* Min 10 seconds for POST */
    }
    
    /* Debug: show timeout being used */
    char timeout_msg[64];
    int n = snprintf(timeout_msg, sizeof(timeout_msg), "+BNCURL: Using timeout %lu ms (%.1f sec)\r\n", 
                    timeout_ms, timeout_ms / 1000.0);
    at_uart_write_locked((const uint8_t*)timeout_msg, n);
    
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
        if (!sd_card_is_mounted()) {
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
    curl_easy_setopt(h, CURLOPT_USERAGENT, BNCURL_USER_AGENT);
#ifdef BNCURL_FORCE_DNS
    curl_easy_setopt(h, CURLOPT_DNS_SERVERS, "8.8.8.8,1.1.1.1");
#endif
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, BNCURL_CONNECT_TIMEOUT_MS);  /* Connection timeout */
    curl_easy_setopt(h, CURLOPT_TIMEOUT_MS, timeout_ms);     /* Auto-calculated timeout */
    
    /* Adjust low speed timeout based on file size */
    long low_speed_time = BNCURL_LOW_SPEED_TIME_DEFAULT; /* Default 2 minutes */
    if (content_length > BNCURL_LARGE_FILE_THRESHOLD) { /* Files > 100MB */
        low_speed_time = BNCURL_LOW_SPEED_TIME_LARGE; /* 5 minutes for large files */
    }
    curl_easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, 1L);       /* 1 byte/sec minimum */
    curl_easy_setopt(h, CURLOPT_LOW_SPEED_TIME, low_speed_time);
    curl_easy_setopt(h, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(h, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(h, CURLOPT_TCP_KEEPIDLE, 120L);
    curl_easy_setopt(h, CURLOPT_TCP_KEEPINTVL, 60L);
    
    /* Additional options to improve TLS reliability and reduce timeout errors */
    curl_easy_setopt(h, CURLOPT_TCP_NODELAY, 1L);          /* Disable Nagle's algorithm */
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);             /* Don't use signals for timeouts */
    curl_easy_setopt(h, CURLOPT_BUFFERSIZE, BNCURL_BUFFER_SIZE);       /* 32KB buffer for better throughput */

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
    
    /* Configure TLS timeouts to reduce harmless timeout errors */
    curl_easy_setopt(h, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    
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

    /* Progress callback for stop detection */
    curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0L);            /* Enable progress */
    curl_easy_setopt(h, CURLOPT_PROGRESSFUNCTION, bncurl_progress_callback);
    curl_easy_setopt(h, CURLOPT_PROGRESSDATA, req);

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

    CURLcode rc = curl_easy_perform(h);
    long http_code = 0;
    if (rc == CURLE_OK) curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &http_code);

    bncurl_last_http_code = (rc == CURLE_OK) ? http_code : -1;
    strncpy(bncurl_last_url, req->url, sizeof(bncurl_last_url) - 1);
    bncurl_last_url[sizeof(bncurl_last_url) - 1] = '\0';
    
    /* Close file if it was opened */
    if (ctx.save_file) {
        fclose(ctx.save_file);
        ctx.save_file = NULL;
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
        at_uart_write_locked((const uint8_t*)"SEND OK\r\n", 9);
        
        /* Restore log levels */
        if (!req->verbose) {
            esp_log_level_set("mbedtls", old_mbedtls_level);
            esp_log_level_set("Dynamic Impl", old_dynamic_impl_level);
        }
        return ESP_AT_RESULT_CODE_OK;
    }

    /* Map “no Content-Length” strict failure */
    if (rc == CURLE_WRITE_ERROR && !ctx.len_announced && !ctx.have_len) {
        const char *e = "\r\n+BNCURL: ERROR length-unknown (no Content-Length)\r\n";
        at_uart_write_locked((const uint8_t*)e, strlen(e));
        
        /* Restore log levels */
        if (!req->verbose) {
            esp_log_level_set("mbedtls", old_mbedtls_level);
            esp_log_level_set("Dynamic Impl", old_dynamic_impl_level);
        }
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* If we failed after announcing LEN (mid-stream), send SEND FAIL */
    if (ctx.len_announced) {
        at_uart_write_locked((const uint8_t*)"SEND FAIL\r\n", 11);
    }
    
    /* Check if operation was stopped by user request */
    if (rc == CURLE_ABORTED_BY_CALLBACK && bncurl_stop_requested) {
        const char *stop_msg = "+BNCURL: Operation stopped by user\r\n";
        at_uart_write_locked((const uint8_t*)stop_msg, strlen(stop_msg));
        
        /* Restore log levels */
        if (!req->verbose) {
            esp_log_level_set("mbedtls", old_mbedtls_level);
            esp_log_level_set("Dynamic Impl", old_dynamic_impl_level);
        }
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    char e2[128];
    n = snprintf(e2, sizeof(e2), "+BNCURL: ERROR %d %s (bytes %lu)\r\n",
                     rc, curl_easy_strerror(rc), (unsigned long)ctx.total_bytes);
    at_uart_write_locked((uint8_t*)e2, n);
    
    /* Restore log levels */
    if (!req->verbose) {
        esp_log_level_set("mbedtls", old_mbedtls_level);
        esp_log_level_set("Dynamic Impl", old_dynamic_impl_level);
    }
    return ESP_AT_RESULT_CODE_ERROR;
}

static void bncurl_worker(void *arg) {
    for (;;) {
        bncurl_req_t *req_ptr = NULL;
        if (xQueueReceive(bncurl_q, &req_ptr, portMAX_DELAY) == pdTRUE && req_ptr) {
            /* Set operation running flags */
            bncurl_operation_running = true;
            bncurl_stop_requested = false;
            
            /* Perform the request */
            req_ptr->result_code = bncurl_perform_internal(req_ptr);
            
            /* Clear operation running flag */
            bncurl_operation_running = false;
            bncurl_stop_requested = false;
            
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
        "  -du <size>       Upload <size> bytes from UART (POST method only)\r\n"
        "  -du <filepath>   Upload file content (POST method only, @ prefix optional)\r\n"
        "  -H <header>      Add custom HTTP header (up to 10 headers)\r\n"
        "  -v               Enable verbose mode (show detailed HTTP transaction)\r\n"
        "Examples:\r\n"
        "  AT+BNCURL=GET,\"http://httpbin.org/get\"       Stream to UART (HTTP)\r\n"
        "  AT+BNCURL=HEAD,\"http://httpbin.org/get\"      Print headers to UART (HTTP)\r\n"
        "  AT+BNCURL=GET,\"http://httpbin.org/get\",-v    Verbose GET request\r\n"
        "  AT+BNCURL=POST,\"http://httpbin.org/post\",-du,\"8\"  Upload 8 bytes from UART\r\n"
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
        "Note: Directories are created automatically if they don't exist\r\n"
        "Control Commands:\r\n"
        "  AT+BNCURL_STOP?                               Query if BNCURL operation is running\r\n"
        "  AT+BNCURL_STOP                                Stop current download/upload operation\r\n"
        "  AT+BNCURL_TIMEOUT?                            Query current timeout setting\r\n"
        "  AT+BNCURL_TIMEOUT=<seconds>                   Set timeout (1-1800 seconds)\r\n"
        "Limits:\r\n"
        "  URL: max 255 characters\r\n"
        "  File paths: max 120 characters (before @ expansion)\r\n"
        "  Headers: max 250 characters, must contain ':'\r\n"
        "  UART upload: max 1MB, numeric values only\r\n"
        "  Max 10 custom headers per request\r\n"
        "  No duplicate parameters allowed\r\n";
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

    /* Validate URL length */
    if (strlen((const char*)url) >= BNCURL_URL_MAX_LEN) {
        const char *e = "+BNCURL: ERROR URL too long (max 255 characters)\r\n";
        at_uart_write_locked((const uint8_t*)e, strlen(e));
        return ESP_AT_RESULT_CODE_ERROR;
    }

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
    char file_path_tmp[BNCURL_FILEPATH_BUFFER_SIZE] = {0};
    bool want_upload = false;
    char upload_param[BNCURL_UPLOAD_PARAM_BUFFER_SIZE] = {0};
    bool upload_from_file = false;
    size_t upload_size = 0;
    bool want_verbose = false;
    
    /* Custom headers storage */
    char headers_list[BNCURL_MAX_HEADERS][BNCURL_HEADER_BUFFER_SIZE];
    int header_count = 0;
    
    /* Parameter validation flags to detect duplicates */
    bool dd_seen = false;
    bool du_seen = false;
    bool v_seen = false;
    
    /* First pass: validate all parameters */
    for (int i = 2; i < para_num; i++) {
        uint8_t *opt = NULL;
        esp_at_para_parse_result_type result = esp_at_get_para_as_str(i, &opt);
        
        if (result != ESP_AT_PARA_PARSE_RESULT_OK || !opt) {
            const char *e = "+BNCURL: ERROR invalid parameter format\r\n";
            at_uart_write_locked((const uint8_t*)e, strlen(e));
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        /* Check for known parameters */
        if (strcasecmp((const char*)opt, "-dd") == 0) {
            if (dd_seen) {
                const char *e = "+BNCURL: ERROR duplicate -dd parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
            dd_seen = true;
            i++; /* Skip next parameter (file path) */
            if (i >= para_num) {
                const char *e = "+BNCURL: ERROR -dd requires file path parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
        } else if (strcasecmp((const char*)opt, "-du") == 0) {
            if (du_seen) {
                const char *e = "+BNCURL: ERROR duplicate -du parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
            /* Validate that -du is only used with POST method */
            if (method != BNCURL_POST) {
                const char *e = "+BNCURL: ERROR -du parameter only valid with POST method\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
            du_seen = true;
            i++; /* Skip next parameter (upload param) */
            if (i >= para_num) {
                const char *e = "+BNCURL: ERROR -du requires parameter (size or file path)\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
        } else if (strcasecmp((const char*)opt, "-H") == 0) {
            i++; /* Skip next parameter (header) */
            if (i >= para_num) {
                const char *e = "+BNCURL: ERROR -H requires header parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
        } else if (strcasecmp((const char*)opt, "-v") == 0) {
            if (v_seen) {
                const char *e = "+BNCURL: ERROR duplicate -v parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
            v_seen = true;
        } else {
            /* Unknown parameter */
            char err_msg[128];
            int n = snprintf(err_msg, sizeof(err_msg), "+BNCURL: ERROR unknown parameter: %s\r\n", (const char*)opt);
            at_uart_write_locked((const uint8_t*)err_msg, n);
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    
    /* Parse all parameters starting from parameter 2 (second pass for actual processing) */
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
                        /* Validate file path length before processing */
                        if (strlen((const char*)path) > BNCURL_FILEPATH_MAX_LEN) { /* Leave room for @/sdcard/ expansion */
                            const char *e = "+BNCURL: ERROR -dd file path too long (max 120 characters)\r\n";
                            at_uart_write_locked((const uint8_t*)e, strlen(e));
                            return ESP_AT_RESULT_CODE_ERROR;
                        }
                        
                        /* Handle @ prefix as SD card mount point shorthand */
                        const char *input_path = (const char*)path;
                        if (input_path[0] == '@') {
                            if (input_path[1] == '/' || input_path[1] == '\0') {
                                /* @/ or @ represents the mount point */
                                if (input_path[1] == '/') {
                                    /* @/path/file.json -> /sdcard/path/file.json */
                                    snprintf(file_path_tmp, sizeof(file_path_tmp)-1, "%s%s", BNCURL_SDCARD_MOUNT_POINT, input_path + 1);
                                } else {
                                    /* @ -> /sdcard */
                                    snprintf(file_path_tmp, sizeof(file_path_tmp)-1, "%s", BNCURL_SDCARD_MOUNT_POINT);
                                }
                            } else {
                                /* @filename -> /sdcard/filename */
                                snprintf(file_path_tmp, sizeof(file_path_tmp)-1, "%s/%s", BNCURL_SDCARD_MOUNT_POINT, input_path + 1);
                            }
                        } else {
                            /* No @ prefix, use path as-is */
                            strncpy(file_path_tmp, input_path, sizeof(file_path_tmp)-1);
                        }
                        file_path_tmp[sizeof(file_path_tmp)-1] = '\0'; /* Ensure null termination */
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
                        /* Validate upload parameter length */
                        if (strlen((const char*)param) > BNCURL_FILEPATH_MAX_LEN) { /* Leave room for @/sdcard/ expansion */
                            const char *e = "+BNCURL: ERROR -du parameter too long (max 120 characters)\r\n";
                            at_uart_write_locked((const uint8_t*)e, strlen(e));
                            return ESP_AT_RESULT_CODE_ERROR;
                        }
                        
                        const char *input_param = (const char*)param;
                        want_upload = true;
                        i++; /* Skip next parameter as it's the upload param */
                        
                        /* Check if it's a file path (starts with @ or is a path) */
                        if (input_param[0] == '@') {
                            /* File upload - handle @ as mount point shorthand */
                            upload_from_file = true;
                            if (input_param[1] == '/' || input_param[1] == '\0') {
                                /* @/ or @ represents the mount point */
                                if (input_param[1] == '/') {
                                    /* @/path/file.bin -> /sdcard/path/file.bin */
                                    snprintf(upload_param, sizeof(upload_param)-1, "%s%s", BNCURL_SDCARD_MOUNT_POINT, input_param + 1);
                                } else {
                                    /* @ -> /sdcard */
                                    snprintf(upload_param, sizeof(upload_param)-1, "%s", BNCURL_SDCARD_MOUNT_POINT);
                                }
                            } else {
                                /* @filename -> /sdcard/filename */
                                snprintf(upload_param, sizeof(upload_param)-1, "%s/%s", BNCURL_SDCARD_MOUNT_POINT, input_param + 1);
                            }
                            upload_param[sizeof(upload_param)-1] = '\0'; /* Ensure null termination */
                        } else if (input_param[0] == '/' || strchr(input_param, '/') != NULL) {
                            /* File path without @ prefix */
                            upload_from_file = true;
                            strncpy(upload_param, input_param, sizeof(upload_param)-1);
                            upload_param[sizeof(upload_param)-1] = '\0';
                        } else {
                            /* UART size parameter - validate it's a valid number */
                            upload_from_file = false;
                            
                            /* Check if parameter is a valid number */
                            const char *p = input_param;
                            bool is_valid_number = true;
                            if (*p == '\0') is_valid_number = false; /* Empty string */
                            
                            while (*p && is_valid_number) {
                                if (*p < '0' || *p > '9') {
                                    is_valid_number = false;
                                }
                                p++;
                            }
                            
                            if (!is_valid_number) {
                                const char *e = "+BNCURL: ERROR -du size must be a valid number\r\n";
                                at_uart_write_locked((const uint8_t*)e, strlen(e));
                                return ESP_AT_RESULT_CODE_ERROR;
                            }
                            
                            upload_size = (size_t)atoi(input_param);
                            
                            /* Validate reasonable size limits (max 1MB for UART input) */
                            if (upload_size > BNCURL_UART_UPLOAD_MAX_SIZE) {
                                const char *e = "+BNCURL: ERROR -du size too large (max 1MB for UART input)\r\n";
                                at_uart_write_locked((const uint8_t*)e, strlen(e));
                                return ESP_AT_RESULT_CODE_ERROR;
                            }
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
                if (i + 1 < para_num && header_count < BNCURL_MAX_HEADERS) {
                    uint8_t *header = NULL;
                    result = esp_at_get_para_as_str(i + 1, &header);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && header) {
                        /* Validate header length */
                        if (strlen((const char*)header) > BNCURL_HEADER_MAX_LEN) {
                            const char *e = "+BNCURL: ERROR -H header too long (max 250 characters)\r\n";
                            at_uart_write_locked((const uint8_t*)e, strlen(e));
                            return ESP_AT_RESULT_CODE_ERROR;
                        }
                        
                        /* Basic header format validation (should contain :) */
                        if (!strchr((const char*)header, ':')) {
                            const char *e = "+BNCURL: ERROR -H header must contain ':' (format: 'Name: Value')\r\n";
                            at_uart_write_locked((const uint8_t*)e, strlen(e));
                            return ESP_AT_RESULT_CODE_ERROR;
                        }
                        
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
            }
        }
    }

    bncurl_req_t *req = (bncurl_req_t*)calloc(1, sizeof(bncurl_req_t));
    if (!req) return ESP_AT_RESULT_CODE_ERROR;
    req->method = method;
    strncpy(req->url, (const char*)url, sizeof(req->url)-1);
    req->save_to_file = want_file;
    req->verbose = want_verbose;
    if (want_file) {
        strncpy(req->save_path, file_path_tmp, sizeof(req->save_path)-1);
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
                if (xSemaphoreTake(data_input_sema, pdMS_TO_TICKS(BNCURL_DATA_INPUT_TIMEOUT_MS))) {
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

    if (xQueueSend(bncurl_q, &req, pdMS_TO_TICKS(BNCURL_QUEUE_SEND_TIMEOUT_MS)) != pdTRUE) {
        vSemaphoreDelete(req->done);
        if (req->headers) curl_slist_free_all(req->headers);
        if (req->upload_data) free(req->upload_data);
        free(req);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Wait for completion with extended timeout for large files (up to 60 minutes) */
    if (xSemaphoreTake(req->done, pdMS_TO_TICKS(BNCURL_OPERATION_TIMEOUT_MS)) != pdTRUE) {
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

    if (xQueueSend(bncurl_q, &req, pdMS_TO_TICKS(BNCURL_QUEUE_SEND_TIMEOUT_MS)) != pdTRUE) {
        vSemaphoreDelete(req->done);
        free(req);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Wait for completion with extended timeout for large files (up to 60 minutes) */
    if (xSemaphoreTake(req->done, pdMS_TO_TICKS(BNCURL_OPERATION_TIMEOUT_MS)) != pdTRUE) {
        vSemaphoreDelete(req->done);
        free(req);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    uint8_t rc = req->result_code;
    vSemaphoreDelete(req->done);
    free(req);
    return rc;
}

/* ======================= BNCURL_TIMEOUT Command ======================= */

static uint8_t at_bncurl_timeout_cmd_test(uint8_t *cmd_name) {
    const char *msg = 
        "Usage:\r\n"
        "  AT+BNCURL_TIMEOUT?                Query current timeout setting\r\n"
        "  AT+BNCURL_TIMEOUT=<seconds>       Set timeout (1-1800 seconds)\r\n"
        "Description:\r\n"
        "  Set timeout for server reaction in seconds. Can be anything between 1 and 1800.\r\n"
        "Examples:\r\n"
        "  AT+BNCURL_TIMEOUT=100             Set timeout to 100 seconds\r\n"
        "  AT+BNCURL_TIMEOUT?                Query current timeout\r\n"
        "  Response: +BNCURL_TIMEOUT: 30     (Timeout is set to 30 seconds)\r\n";
    at_uart_write_locked((const uint8_t*)msg, strlen(msg));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_timeout_cmd_query(uint8_t *cmd_name) {
    char out[64];
    snprintf(out, sizeof(out), "+BNCURL_TIMEOUT: %lu\r\n", (unsigned long)bncurl_timeout_seconds);
    at_uart_write_locked((uint8_t*)out, strlen(out));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_timeout_cmd_setup(uint8_t para_num) {
    if (para_num != 1) {
        const char *e = "+BNCURL_TIMEOUT: ERROR invalid parameter count\r\n";
        at_uart_write_locked((const uint8_t*)e, strlen(e));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    int32_t timeout_value = 0;
    if (esp_at_get_para_as_digit(0, &timeout_value) != ESP_AT_PARA_PARSE_RESULT_OK) {
        const char *e = "+BNCURL_TIMEOUT: ERROR invalid parameter format (must be number)\r\n";
        at_uart_write_locked((const uint8_t*)e, strlen(e));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Validate timeout range */
    if (timeout_value < BNCURL_TIMEOUT_MIN_SECONDS || timeout_value > BNCURL_TIMEOUT_MAX_SECONDS) {
        char err_msg[128];
        int n = snprintf(err_msg, sizeof(err_msg), 
                        "+BNCURL_TIMEOUT: ERROR timeout out of range (%d-%d seconds)\r\n",
                        BNCURL_TIMEOUT_MIN_SECONDS, BNCURL_TIMEOUT_MAX_SECONDS);
        at_uart_write_locked((const uint8_t*)err_msg, n);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Set the new timeout value */
    bncurl_timeout_seconds = (uint32_t)timeout_value;
    
    /* Confirm the setting */
    char msg[64];
    int n = snprintf(msg, sizeof(msg), "+BNCURL_TIMEOUT: %lu\r\n", (unsigned long)bncurl_timeout_seconds);
    at_uart_write_locked((const uint8_t*)msg, n);
    
    return ESP_AT_RESULT_CODE_OK;
}

/* ================= AT+BNCURL_STOP command implementation ================= */

static uint8_t at_bncurl_stop_cmd_test(uint8_t *cmd_name) {
    const char *msg =
        "Usage:\r\n"
        "  AT+BNCURL_STOP?    Query whether a BNCURL operation is currently running\r\n"
        "  AT+BNCURL_STOP     Stop the current BNCURL download/upload operation\r\n"
        "Response:\r\n"
        "  +BNCURL_STOP:      (operation stopped successfully)\r\n"
        "  OK                 \r\n"
        "  or\r\n"
        "  +BNCURL_STOP:      \r\n"
        "  ERROR              (no operation running or stop failed)\r\n"
        "Note: This command only works during active file download/upload operations\r\n";
    at_uart_write_locked((const uint8_t*)msg, strlen(msg));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_stop_cmd_query(uint8_t *cmd_name) {
    char msg[64];
    int n = snprintf(msg, sizeof(msg), "+BNCURL_STOP: %s\r\n", 
                    bncurl_operation_running ? "RUNNING" : "IDLE");
    at_uart_write_locked((const uint8_t*)msg, n);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_stop_cmd_exe(uint8_t *cmd_name) {
    /* Check if there is an operation to stop */
    if (!bncurl_operation_running) {
        const char *err_msg = "+BNCURL_STOP: \r\n";
        at_uart_write_locked((const uint8_t*)err_msg, strlen(err_msg));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Signal the operation to stop */
    bncurl_stop_requested = true;
    
    /* Confirm stop request initiated */
    const char *ok_msg = "+BNCURL_STOP: \r\n";
    at_uart_write_locked((const uint8_t*)ok_msg, strlen(ok_msg));
    
    return ESP_AT_RESULT_CODE_OK;
}

/* ----------------------- Command table & init ----------------------- */

static const esp_at_cmd_struct at_custom_cmd[] = {
    {"+BNSD_MOUNT",     at_bnsd_mount_cmd_test, at_bnsd_mount_cmd_query, NULL,             at_bnsd_mount_cmd_exe},
    {"+BNSD_UNMOUNT",   at_bnsd_unmount_cmd_test, at_bnsd_unmount_cmd_query, NULL,         at_bnsd_unmount_cmd_exe},
    {"+BNSD_FORMAT",    at_bnsd_format_cmd_test, at_bnsd_format_cmd_query, NULL,           at_bnsd_format_cmd_exe},
    {"+BNSD_SPACE",     at_bnsd_space_cmd_test, at_bnsd_space_cmd_query, NULL,             at_bnsd_space_cmd_exe},
    {"+BNCURL",         at_bncurl_cmd_test,     at_bncurl_cmd_query,  at_bncurl_cmd_setup, at_bncurl_cmd_exe},
    {"+BNCURL_TIMEOUT", at_bncurl_timeout_cmd_test, at_bncurl_timeout_cmd_query, at_bncurl_timeout_cmd_setup, NULL},
    {"+BNCURL_STOP",    at_bncurl_stop_cmd_test, at_bncurl_stop_cmd_query, NULL,          at_bncurl_stop_cmd_exe},
    /* add further custom AT commands here */
};

bool esp_at_custom_cmd_register(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    sd_card_init();
    
    bool ok = esp_at_custom_cmd_array_regist(at_custom_cmd, sizeof(at_custom_cmd) / sizeof(esp_at_cmd_struct));
    if (!ok) return false;

    if (!at_uart_lock) at_uart_lock = xSemaphoreCreateMutex();
    if (!data_input_sema) data_input_sema = xSemaphoreCreateBinary();
    if (!bncurl_q)     bncurl_q = xQueueCreate(BNCURL_QUEUE_SIZE, sizeof(bncurl_req_t*));
    if (!bncurl_task) {
        /* TLS + libcurl + printf ==> give it a big stack; tune later */
        xTaskCreatePinnedToCore(bncurl_worker, "bncurl", BNCURL_TASK_STACK_SIZE, NULL, BNCURL_TASK_PRIORITY, &bncurl_task, 0);
    }
    return true;
}

ESP_AT_CMD_SET_INIT_FN(esp_at_custom_cmd_register, 1);
