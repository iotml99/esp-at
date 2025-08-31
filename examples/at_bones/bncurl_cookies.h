/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCURL_COOKIES_H
#define BNCURL_COOKIES_H

#include <stdbool.h>
#include <stddef.h>
#include <curl/curl.h>
#include "bncurl_config.h"
#include "bncurl.h"

/**
 * @brief Cookie structure for storing parsed cookie information
 */
typedef struct {
    char name[BNCURL_MAX_COOKIE_NAME_LENGTH + 1];
    char value[BNCURL_MAX_COOKIE_VALUE_LENGTH + 1];
    char domain[BNCURL_MAX_COOKIE_DOMAIN_LENGTH + 1];
    char path[64];
    bool secure;
    bool http_only;
    long expires;
} bncurl_cookie_t;

/**
 * @brief Cookie context for managing cookies during requests
 */
typedef struct {
    bncurl_cookie_t cookies[BNCURL_MAX_COOKIES_COUNT];
    int cookie_count;
    char save_file_path[BNCURL_MAX_COOKIE_FILE_PATH + 1];
    bool save_to_file;
    bool send_to_uart;
} bncurl_cookie_context_t;

/**
 * @brief Load cookies from a file for sending with requests (-b option)
 * 
 * @param curl_handle CURL handle to configure with cookies
 * @param cookie_file_path Path to cookie file to load
 * @return true on success, false on failure
 */
bool bncurl_cookies_load_from_file(CURL *curl_handle, const char *cookie_file_path);

/**
 * @brief Configure cookie saving for a request (-c option)
 * 
 * @param curl_handle CURL handle to configure with cookie saving
 * @param cookie_file_path Path where to save cookies (can be NULL for UART only)
 * @param cookie_ctx Cookie context for storing cookies
 * @return true on success, false on failure
 */
bool bncurl_cookies_configure_saving(CURL *curl_handle, const char *cookie_file_path, 
                                    bncurl_cookie_context_t *cookie_ctx);

/**
 * @brief Cookie write callback for capturing cookies during requests
 * 
 * @param cookie Cookie string from libcurl
 * @param userdata Cookie context for storing cookies
 * @return CURLCOOKIEOUTPUT_OK on success
 */
int bncurl_cookies_write_callback(const char *cookie, void *userdata);

/**
 * @brief Stream captured cookies to UART
 * 
 * @param cookie_ctx Cookie context containing captured cookies
 */
void bncurl_cookies_stream_to_uart(const bncurl_cookie_context_t *cookie_ctx);

/**
 * @brief Save captured cookies to file
 * 
 * @param cookie_ctx Cookie context containing captured cookies
 * @return true on success, false on failure
 */
bool bncurl_cookies_save_to_file(const bncurl_cookie_context_t *cookie_ctx);

/**
 * @brief Initialize cookie context
 * 
 * @param cookie_ctx Cookie context to initialize
 * @param save_file_path Path for saving cookies (can be NULL)
 */
void bncurl_cookies_init_context(bncurl_cookie_context_t *cookie_ctx, const char *save_file_path);

/**
 * @brief Clean up cookie context
 * 
 * @param cookie_ctx Cookie context to clean up
 */
void bncurl_cookies_cleanup_context(bncurl_cookie_context_t *cookie_ctx);

/**
 * @brief Parse cookie string and add to context
 * 
 * @param cookie_ctx Cookie context to add cookie to
 * @param cookie_string Raw cookie string from Set-Cookie header
 * @return true on success, false on failure
 */
bool bncurl_cookies_parse_and_add(bncurl_cookie_context_t *cookie_ctx, const char *cookie_string);

/**
 * @brief Validate cookie file path and create directories if needed
 * 
 * @param cookie_file_path Path to validate and prepare
 * @return true on success, false on failure
 */
bool bncurl_cookies_validate_file_path(const char *cookie_file_path);

/**
 * @brief Cookie header callback for capturing Set-Cookie headers during requests
 * 
 * @param buffer Header data buffer
 * @param size Size of each element  
 * @param nitems Number of elements
 * @param userdata Cookie context for storing cookies
 * @return Number of bytes processed
 */
size_t cookie_header_callback(char *buffer, size_t size, size_t nitems, void *userdata);

#endif /* BNCURL_COOKIES_H */
