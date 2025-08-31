/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCURL_COMMON_H
#define BNCURL_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <curl/curl.h>
#include "bncurl.h"
#include "bncurl_methods.h"
#include "bncurl_cookies.h"

// Common context structure for shared curl operations
typedef struct {
    bncurl_context_t *ctx;              // Main BNCURL context
    bncurl_stream_context_t *stream;    // Streaming context
    bncurl_cookie_context_t *cookies;   // Cookie context for handling cookies
    char *http_date_header;             // HTTP Date header for kill switch (dynamically allocated)
} bncurl_common_context_t;

/**
 * @brief Common write callback for curl with dual-buffer streaming
 * 
 * @param contents Data received from curl
 * @param size Size of each element
 * @param nmemb Number of elements
 * @param userdata Pointer to bncurl_common_context_t
 * @return Number of bytes processed
 */
size_t bncurl_common_write_callback(void *contents, size_t size, size_t nmemb, void *userdata);

/**
 * @brief Common header callback for curl
 * 
 * @param buffer Header data
 * @param size Size of each element
 * @param nitems Number of elements
 * @param userdata Pointer to bncurl_common_context_t
 * @return Number of bytes processed
 */
size_t bncurl_common_header_callback(char *buffer, size_t size, size_t nitems, void *userdata);

/**
 * @brief Combined header callback for curl (handles both content-length and cookies)
 * 
 * @param buffer Header data
 * @param size Size of each element
 * @param nitems Number of elements
 * @param userdata Pointer to bncurl_common_context_t
 * @return Number of bytes processed
 */
size_t bncurl_combined_header_callback(char *buffer, size_t size, size_t nitems, void *userdata);

/**
 * @brief Common progress callback for curl
 * 
 * @param clientp Pointer to bncurl_common_context_t
 * @param dltotal Total download size
 * @param dlnow Current download size
 * @param ultotal Total upload size
 * @param ulnow Current upload size
 * @return 0 to continue, non-zero to abort
 */
int bncurl_common_progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                   curl_off_t ultotal, curl_off_t ulnow);

/**
 * @brief Execute HTTP request using common functionality
 * 
 * @param ctx BNCURL context
 * @param stream Streaming context
 * @param method HTTP method (GET, POST, HEAD)
 * @return true on success, false on failure
 */
bool bncurl_common_execute_request(bncurl_context_t *ctx, bncurl_stream_context_t *stream, 
                                   const char *method);

/**
 * @brief Get content length via HEAD request
 * 
 * @param ctx BNCURL context
 * @param content_length Pointer to store the content length
 * @return true if content length was successfully retrieved, false otherwise
 */
bool bncurl_common_get_content_length(bncurl_context_t *ctx, size_t *content_length);

#endif /* BNCURL_COMMON_H */
