/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCURL_CONFIG_H
#define BNCURL_CONFIG_H

#include <stdint.h>

/**
 * @brief BNCURL Configuration and Limits
 * 
 * This header defines various configuration constants and limits
 * for BNCURL parameters based on industry standards and ESP32 constraints.
 */

/* ==================== URL Configuration ==================== */
/** Maximum URL length (based on spec examples like weather API) */
#define BNCURL_MAX_URL_LENGTH           256

/** Maximum hostname length (RFC 1035 limit) */
#define BNCURL_MAX_HOSTNAME_LENGTH      253

/** Maximum path length in URL */
#define BNCURL_MAX_URL_PATH_LENGTH      256

/** Maximum query string length */
#define BNCURL_MAX_QUERY_STRING_LENGTH  256

/* ==================== HTTP Method Configuration ==================== */
/** Maximum method name length */
#define BNCURL_MAX_METHOD_LENGTH        8

/** Supported HTTP methods count */
#define BNCURL_SUPPORTED_METHODS_COUNT  3

/* ==================== Header Configuration ==================== */
/** Maximum number of custom headers allowed (spec mentions 2-5 in real life, 16 for safety) */
#define BNCURL_MAX_HEADERS_COUNT        16

/** Maximum length of a single header (name + value + ": ") */
#define BNCURL_MAX_HEADER_LENGTH        256

/** Maximum header name length (practical limit) */
#define BNCURL_MAX_HEADER_NAME_LENGTH   64

/** Maximum header value length */
#define BNCURL_MAX_HEADER_VALUE_LENGTH  180

/** Total maximum headers buffer size */
#define BNCURL_MAX_TOTAL_HEADERS_SIZE   (BNCURL_MAX_HEADERS_COUNT * BNCURL_MAX_HEADER_LENGTH)

/* ==================== File Path Configuration ==================== */
/** Maximum file path length (ESP32 SPIFFS/FAT32 practical limit) */
#define BNCURL_MAX_FILE_PATH_LENGTH     128

/** Maximum filename length (FAT32 limit with long file names) */
#define BNCURL_MAX_FILENAME_LENGTH      32

/** Maximum directory depth */
#define BNCURL_MAX_DIRECTORY_DEPTH      8

/* ==================== Data Transfer Configuration ==================== */
/** Maximum data upload size via UART (reasonable for ESP32 memory) */
#define BNCURL_MAX_UART_UPLOAD_SIZE     (512 * 1024)  // 512KB

/** Maximum file upload size from SD card (practical limit) */
#define BNCURL_MAX_FILE_UPLOAD_SIZE     (16 * 1024 * 1024)  // 16MB

/** Maximum download size to SD card (based on typical SD card sizes) */
#define BNCURL_MAX_DOWNLOAD_SIZE        (512 * 1024 * 1024)  // 512MB

/** Chunk size for data transfer (spec mentions 512 byte chunks) */
#define BNCURL_TRANSFER_CHUNK_SIZE      512

/** UART buffer size for streaming (spec mentions 512 byte chunks) */
#define BNCURL_UART_BUFFER_SIZE         512

/* ==================== Cookie Configuration ==================== */
/** Maximum cookie file path length */
#define BNCURL_MAX_COOKIE_FILE_PATH     BNCURL_MAX_FILE_PATH_LENGTH

/** Maximum number of cookies per file (reasonable for session management) */
#define BNCURL_MAX_COOKIES_COUNT        16

/** Maximum cookie name length */
#define BNCURL_MAX_COOKIE_NAME_LENGTH   32

/** Maximum cookie value length */
#define BNCURL_MAX_COOKIE_VALUE_LENGTH  128

/** Maximum cookie domain length */
#define BNCURL_MAX_COOKIE_DOMAIN_LENGTH 64

/* ==================== Range Request Configuration ==================== */
/** Maximum range string length (e.g., "0-2097151") */
#define BNCURL_MAX_RANGE_STRING_LENGTH  24

/** Maximum range start value (32-bit limit) */
#define BNCURL_MAX_RANGE_START          0xFFFFFFFF

/** Maximum range end value (32-bit limit) */
#define BNCURL_MAX_RANGE_END            0xFFFFFFFF

/* ==================== Timeout Configuration ==================== */
/** Default server response timeout (seconds) - if no data received for this duration, abort
 *  This is NOT a total download timeout, but a server inactivity timeout */
#define BNCURL_DEFAULT_TIMEOUT          30

/** Minimum server response timeout value (seconds) - spec minimum */
#define BNCURL_MIN_TIMEOUT              1

/** Maximum server response timeout value (seconds) - spec maximum */
#define BNCURL_MAX_TIMEOUT              120

/** DNS resolution timeout (seconds) */
#define BNCURL_DNS_TIMEOUT              10

/** TLS handshake timeout (seconds) */
#define BNCURL_TLS_TIMEOUT              15

/** File download low speed timeout (seconds) - for SD card operations */
#define BNCURL_FILE_LOW_SPEED_TIME      120

/** File download minimum speed (bytes/sec) - very lenient for SD card writes */
#define BNCURL_FILE_LOW_SPEED_LIMIT     10

/** File download total timeout (seconds) - for very large files */
#define BNCURL_FILE_TOTAL_TIMEOUT       7200

/* ==================== Memory Configuration ==================== */
/** Maximum memory allocation for single operation (ESP32 constraint) */
#define BNCURL_MAX_MEMORY_ALLOCATION    (128 * 1024)  // 128KB

/** HTTP response buffer size */
#define BNCURL_HTTP_RESPONSE_BUFFER     4096

/** HTTP request buffer size */
#define BNCURL_HTTP_REQUEST_BUFFER      2048

/** Parser working buffer size */
#define BNCURL_PARSER_BUFFER_SIZE       512

/* ==================== SSL/TLS Configuration ==================== */
/** Maximum certificate size (typical certificate size) */
#define BNCURL_MAX_CERT_SIZE            (4 * 1024)  // 4KB

/** Maximum CA bundle size (reasonable for ESP32) */
#define BNCURL_MAX_CA_BUNDLE_SIZE       (16 * 1024)  // 16KB

/** SSL verification depth */
#define BNCURL_SSL_VERIFY_DEPTH         3

/* ==================== UART Configuration ==================== */
/** Default UART baud rate */
#define BNCURL_DEFAULT_UART_BAUD        115200

/** Maximum UART baud rate (spec requirement) */
#define BNCURL_MAX_UART_BAUD            3000000

/** UART flow control characters */
#define BNCURL_UART_FLOW_STOP           '-'
#define BNCURL_UART_FLOW_START          '+'

/** UART data prompt character */
#define BNCURL_UART_DATA_PROMPT         '>'

/* ==================== WPS Configuration ==================== */
/** Minimum WPS timeout (seconds) */
#define BNCURL_WPS_MIN_TIMEOUT          1

/** Maximum WPS timeout (seconds) */
#define BNCURL_WPS_MAX_TIMEOUT          300

/** WPS cancellation value */
#define BNCURL_WPS_CANCEL               0

/* ==================== Certificate Flash Configuration ==================== */
/** Certificate flash address alignment */
#define BNCURL_CERT_FLASH_ALIGNMENT     0x1000  // 4KB alignment

/** Maximum certificate flash size */
#define BNCURL_MAX_CERT_FLASH_SIZE      BNCURL_MAX_CERT_SIZE

/* ==================== SD Card Configuration ==================== */
/** SD card file system type */
#define BNCURL_SD_FILESYSTEM            "FAT32"

/** SD card mount point */
#define BNCURL_SD_MOUNT_POINT           "/sdcard"

/** Maximum SD card size (bytes) for space reporting */
#define BNCURL_MAX_SD_CARD_SIZE         (64ULL * 1024 * 1024 * 1024)  // 64GB

/* ==================== Protocol Configuration ==================== */
/** HTTP version string */
#define BNCURL_HTTP_VERSION             "1.1"

/** User-Agent string maximum length */
#define BNCURL_MAX_USER_AGENT_LENGTH    128

/** Default User-Agent (spec mentions ESP32-based) */
#define BNCURL_DEFAULT_USER_AGENT       "ESP32-BN-Module/1.0"

/** Maximum redirect count (automatic redirect following) */
#define BNCURL_MAX_REDIRECTS            5

/* ==================== Error Configuration ==================== */
/** Maximum error message length */
#define BNCURL_MAX_ERROR_MESSAGE_LENGTH 128

/** Maximum error code value */
#define BNCURL_MAX_ERROR_CODE           999

/* ==================== Response Format Configuration ==================== */
/** Length indicator format prefix */
#define BNCURL_LEN_PREFIX               "+LEN:"

/** Data chunk format prefix */
#define BNCURL_POST_PREFIX              "+POST:"

/** Transfer completion message */
#define BNCURL_SEND_OK_MSG              "SEND OK"

/** Progress report prefix */
#define BNCURL_PROGRESS_PREFIX          "+BNCURL_PROG:"

/** Stop command response prefix */
#define BNCURL_STOP_PREFIX              "+BNCURL_STOP:"

/* ==================== Validation Limits ==================== */
/** Maximum command line length */
#define BNCURL_MAX_COMMAND_LENGTH       4096

/** Maximum parameter count per command */
#define BNCURL_MAX_PARAMETERS_COUNT     32

/** Maximum parameter value length */
#define BNCURL_MAX_PARAMETER_LENGTH     512

/* ==================== Performance Configuration ==================== */
/** TCP socket receive buffer size (optimized for ESP32) */
#define BNCURL_TCP_RECV_BUFFER_SIZE     8192

/** TCP socket send buffer size (optimized for ESP32) */
#define BNCURL_TCP_SEND_BUFFER_SIZE     4096

/** Maximum concurrent connections (spec indicates single connection) */
#define BNCURL_MAX_CONCURRENT_CONNECTIONS 1

/** Connection keep-alive timeout (seconds) */
#define BNCURL_KEEP_ALIVE_TIMEOUT       30

/* ==================== Debug Configuration ==================== */
/** Maximum verbose output line length (for -v option) */
#define BNCURL_MAX_VERBOSE_LINE_LENGTH  128

/** Maximum log message length */
#define BNCURL_MAX_LOG_MESSAGE_LENGTH   256

/* ==================== Port Configuration ==================== */
/** Default HTTP port */
#define BNCURL_DEFAULT_HTTP_PORT        80

/** Default HTTPS port */
#define BNCURL_DEFAULT_HTTPS_PORT       443

/** Maximum port number */
#define BNCURL_MAX_PORT_NUMBER          65535

/* ==================== Content Type Configuration ==================== */
/** Maximum Content-Type length */
#define BNCURL_MAX_CONTENT_TYPE_LENGTH  64

/** Default Content-Type for POST */
#define BNCURL_DEFAULT_CONTENT_TYPE     "application/octet-stream"

/* ==================== Progress Monitoring Configuration ==================== */
/** Progress update interval (bytes) - reasonable for ESP32 */
#define BNCURL_PROGRESS_UPDATE_INTERVAL 4096

/** Progress report format buffer size (for "bytes/total" format) */
#define BNCURL_PROGRESS_BUFFER_SIZE     32

/* ==================== Utility Macros ==================== */
/** Check if string length is within limit */
#define BNCURL_IS_VALID_LENGTH(str, max_len) \
    ((str) != NULL && strlen(str) <= (max_len))

/** Check if numeric value is within range */
#define BNCURL_IS_VALID_RANGE(val, min_val, max_val) \
    ((val) >= (min_val) && (val) <= (max_val))

/** Calculate buffer size with null terminator */
#define BNCURL_BUFFER_SIZE(len) ((len) + 1)

/** Align size to 4-byte boundary (ESP32 optimization) */
#define BNCURL_ALIGN_SIZE(size) (((size) + 3) & ~3)

#endif /* BNCURL_CONFIG_H */
