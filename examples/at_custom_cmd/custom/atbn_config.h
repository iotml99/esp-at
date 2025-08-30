/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ================= BNCURL Configuration Limits ================= */

/* URL and path limits */
#define BNCURL_URL_MAX_LEN              255     /* Maximum URL length */
#define BNCURL_FILEPATH_MAX_LEN         120     /* Maximum file path length (before @ expansion) */
#define BNCURL_FILEPATH_BUFFER_SIZE     128     /* Internal file path buffer size */
#define BNCURL_UPLOAD_PARAM_BUFFER_SIZE 128     /* Internal upload parameter buffer size */

/* HTTP header limits */
#define BNCURL_HEADER_MAX_LEN           250     /* Maximum individual header length */
#define BNCURL_MAX_HEADERS              10      /* Maximum number of custom headers per request */
#define BNCURL_HEADER_BUFFER_SIZE       256     /* Internal header buffer size */

/* Upload limits */
#define BNCURL_UART_UPLOAD_MAX_SIZE     (1024 * 1024)  /* 1MB - Maximum UART upload size */
#define BNCURL_UART_CHUNK_SIZE          1024            /* UART output chunk size */

/* Timeout configuration */
#define BNCURL_DEFAULT_TIMEOUT_MS       120000L         /* Default timeout (2 minutes) */
#define BNCURL_HEAD_TIMEOUT_MS          60000L          /* HEAD request timeout (1 minute) */
#define BNCURL_POST_TIMEOUT_MS          300000L         /* POST request timeout (5 minutes) */
#define BNCURL_CONNECT_TIMEOUT_MS       30000L          /* Connection timeout (30 seconds) */
#define BNCURL_LOW_SPEED_TIME_DEFAULT   120L            /* Low speed timeout (2 minutes) */
#define BNCURL_LOW_SPEED_TIME_LARGE     300L            /* Low speed timeout for large files (5 minutes) */
#define BNCURL_LARGE_FILE_THRESHOLD     (100 * 1024 * 1024)  /* 100MB threshold for large files */

/* Buffer sizes */
#define BNCURL_BUFFER_SIZE              32768L          /* libcurl buffer size (32KB) */
#define BNCURL_DEBUG_MSG_SIZE           128             /* Debug message buffer size */
#define BNCURL_ERROR_MSG_SIZE           128             /* Error message buffer size */

/* SD card mount point */
#define BNCURL_SDCARD_MOUNT_POINT       "/sdcard"       /* Default SD card mount point */

/* Task configuration */
#define BNCURL_TASK_STACK_SIZE          16384           /* BNCURL worker task stack size */
#define BNCURL_TASK_PRIORITY            5               /* BNCURL worker task priority */
#define BNCURL_QUEUE_SIZE               2               /* Request queue size */

/* Timeouts for operations */
#define BNCURL_QUEUE_SEND_TIMEOUT_MS    100             /* Queue send timeout */
#define BNCURL_OPERATION_TIMEOUT_MS     3600000         /* Maximum operation timeout (60 minutes) */
#define BNCURL_DATA_INPUT_TIMEOUT_MS    30000           /* Data input timeout (30 seconds) */

/* Cookie limits */
#define BNCURL_MAX_COOKIES              5               /* Maximum number of cookie files per request */
#define BNCURL_COOKIE_BUFFER_SIZE       128             /* Cookie file path buffer size */

/* Range limits */
#define BNCURL_RANGE_BUFFER_SIZE        32              /* Range specification buffer size */

/* User agent string */
#define BNCURL_USER_AGENT               "esp-at-libcurl/1.0"

/* Speed calculation constants */
#define BNCURL_MIN_SPEED_BYTES_PER_SEC  (50 * 1024)     /* 50 KB/s - conservative minimum speed */
#define BNCURL_BASE_TIMEOUT_MS          60000L          /* Base timeout for connection setup */
#define BNCURL_MAX_TIMEOUT_MS           3600000L        /* Maximum timeout (60 minutes) */
#define BNCURL_MIN_TIMEOUT_MS           300000L         /* Minimum timeout (5 minutes) */
#define BNCURL_TIMEOUT_SAFETY_MARGIN    2               /* 2x safety margin for timeout calculations */

/* BNCURL_TIMEOUT command limits */
#define BNCURL_TIMEOUT_MIN_SECONDS      1               /* Minimum timeout setting (1 second) */
#define BNCURL_TIMEOUT_MAX_SECONDS      1800            /* Maximum timeout setting (30 minutes) */
#define BNCURL_TIMEOUT_DEFAULT_SECONDS  30              /* Default timeout setting (30 seconds) */

#ifdef __cplusplus
}
#endif
