/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bncurl_cookies.h"
#include "bnsd.h"
#include "esp_at.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

static const char *TAG = "BNCURL_COOKIES";

// Forward declaration for static helper function
static void bncurl_cookies_stream_single_to_uart(const bncurl_cookie_t *cookie);

// Cookie write callback for libcurl
size_t cookie_header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    bncurl_cookie_context_t *cookie_ctx = (bncurl_cookie_context_t *)userdata;
    size_t total_size = size * nitems;
    
    // Look for Set-Cookie headers
    if (strncasecmp(buffer, "Set-Cookie:", 11) == 0) {
        char *cookie_start = buffer + 11;
        while (*cookie_start == ' ' || *cookie_start == '\t') cookie_start++; // Skip whitespace
        
        // Create null-terminated cookie string
        char cookie_string[512];
        size_t cookie_len = total_size - (cookie_start - buffer);
        if (cookie_len > sizeof(cookie_string) - 1) {
            cookie_len = sizeof(cookie_string) - 1;
        }
        
        memcpy(cookie_string, cookie_start, cookie_len);
        cookie_string[cookie_len] = '\0';
        
        // Remove trailing CRLF
        char *end = cookie_string + strlen(cookie_string) - 1;
        while (end >= cookie_string && (*end == '\r' || *end == '\n')) {
            *end = '\0';
            end--;
        }
        
        if (strlen(cookie_string) > 0) {
            ESP_LOGI(TAG, "Received Set-Cookie: %s", cookie_string);
            bncurl_cookies_parse_and_add(cookie_ctx, cookie_string);
        }
    }
    
    return total_size;
}

bool bncurl_cookies_load_from_file(CURL *curl_handle, const char *cookie_file_path)
{
    if (!curl_handle || !cookie_file_path || strlen(cookie_file_path) == 0) {
        ESP_LOGE(TAG, "Invalid parameters for cookie loading");
        return false;
    }
    
    ESP_LOGI(TAG, "Loading cookies from file: %s", cookie_file_path);
    
    // Check if file exists
    struct stat file_stat;
    if (stat(cookie_file_path, &file_stat) != 0) {
        ESP_LOGE(TAG, "Cookie file does not exist: %s", cookie_file_path);
        return false;
    }
    
    // Use libcurl's cookie file functionality
    CURLcode res = curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, cookie_file_path);
    if (res != CURLE_OK) {
        ESP_LOGE(TAG, "Failed to set cookie file: %s", curl_easy_strerror(res));
        return false;
    }
    
    ESP_LOGI(TAG, "Cookies loaded from file: %s", cookie_file_path);
    return true;
}

bool bncurl_cookies_configure_saving(CURL *curl_handle, const char *cookie_file_path,
                                    bncurl_cookie_context_t *cookie_ctx)
{
    if (!curl_handle || !cookie_ctx) {
        ESP_LOGE(TAG, "Invalid parameters for cookie saving");
        return false;
    }
    
    // Initialize cookie context
    bncurl_cookies_init_context(cookie_ctx, cookie_file_path);
    
    // Configure libcurl to capture cookies
    curl_easy_setopt(curl_handle, CURLOPT_COOKIEJAR, ""); // Enable cookie engine
    
    // Set custom header callback to capture Set-Cookie headers
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, cookie_header_callback);
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, cookie_ctx);
    
    ESP_LOGI(TAG, "Cookie saving configured. File: %s, UART: %s", 
             cookie_ctx->save_to_file ? cookie_ctx->save_file_path : "none",
             cookie_ctx->send_to_uart ? "yes" : "no");
    
    return true;
}

void bncurl_cookies_init_context(bncurl_cookie_context_t *cookie_ctx, const char *save_file_path)
{
    if (!cookie_ctx) {
        return;
    }
    
    memset(cookie_ctx, 0, sizeof(bncurl_cookie_context_t));
    
    if (save_file_path && strlen(save_file_path) > 0) {
        strncpy(cookie_ctx->save_file_path, save_file_path, BNCURL_MAX_COOKIE_FILE_PATH);
        cookie_ctx->save_file_path[BNCURL_MAX_COOKIE_FILE_PATH] = '\0';
        cookie_ctx->save_to_file = true;
    } else {
        cookie_ctx->save_to_file = false;
    }
    
    // Always send cookies to UART for immediate host access
    cookie_ctx->send_to_uart = true;
    
    ESP_LOGI(TAG, "Cookie context initialized. Save to file: %s, Send to UART: %s",
             cookie_ctx->save_to_file ? "yes" : "no",
             cookie_ctx->send_to_uart ? "yes" : "no");
}

void bncurl_cookies_cleanup_context(bncurl_cookie_context_t *cookie_ctx)
{
    if (!cookie_ctx) {
        return;
    }
    
    // Save cookies to file if configured (cookies already streamed to UART individually)
    if (cookie_ctx->cookie_count > 0 && cookie_ctx->save_to_file) {
        bncurl_cookies_save_to_file(cookie_ctx);
    }
    
    memset(cookie_ctx, 0, sizeof(bncurl_cookie_context_t));
}

bool bncurl_cookies_parse_and_add(bncurl_cookie_context_t *cookie_ctx, const char *cookie_string)
{
    if (!cookie_ctx || !cookie_string || cookie_ctx->cookie_count >= BNCURL_MAX_COOKIES_COUNT) {
        ESP_LOGE(TAG, "Cannot add cookie: invalid context or maximum cookies reached");
        return false;
    }
    
    bncurl_cookie_t *cookie = &cookie_ctx->cookies[cookie_ctx->cookie_count];
    memset(cookie, 0, sizeof(bncurl_cookie_t));
    
    // Parse cookie string: "name=value; attribute=value; ..."
    char cookie_copy[512];
    strncpy(cookie_copy, cookie_string, sizeof(cookie_copy) - 1);
    cookie_copy[sizeof(cookie_copy) - 1] = '\0';
    
    // Parse name=value pair (first part before semicolon)
    char *token = strtok(cookie_copy, ";");
    if (token) {
        char *equals = strchr(token, '=');
        if (equals) {
            *equals = '\0';
            
            // Extract name
            char *name = token;
            while (*name == ' ' || *name == '\t') name++; // Skip leading whitespace
            strncpy(cookie->name, name, BNCURL_MAX_COOKIE_NAME_LENGTH);
            cookie->name[BNCURL_MAX_COOKIE_NAME_LENGTH] = '\0';
            
            // Extract value
            char *value = equals + 1;
            while (*value == ' ' || *value == '\t') value++; // Skip leading whitespace
            strncpy(cookie->value, value, BNCURL_MAX_COOKIE_VALUE_LENGTH);
            cookie->value[BNCURL_MAX_COOKIE_VALUE_LENGTH] = '\0';
        }
    }
    
    // Parse attributes
    while ((token = strtok(NULL, ";")) != NULL) {
        while (*token == ' ' || *token == '\t') token++; // Skip leading whitespace
        
        if (strncasecmp(token, "Domain=", 7) == 0) {
            strncpy(cookie->domain, token + 7, BNCURL_MAX_COOKIE_DOMAIN_LENGTH);
            cookie->domain[BNCURL_MAX_COOKIE_DOMAIN_LENGTH] = '\0';
        } else if (strncasecmp(token, "Path=", 5) == 0) {
            strncpy(cookie->path, token + 5, sizeof(cookie->path) - 1);
            cookie->path[sizeof(cookie->path) - 1] = '\0';
        } else if (strcasecmp(token, "Secure") == 0) {
            cookie->secure = true;
        } else if (strcasecmp(token, "HttpOnly") == 0) {
            cookie->http_only = true;
        } else if (strncasecmp(token, "Expires=", 8) == 0) {
            // Parse expires date (simplified - could be enhanced)
            cookie->expires = 0; // For now, just mark as session cookie
        }
    }
    
    cookie_ctx->cookie_count++;
    
    ESP_LOGI(TAG, "Added cookie: %s=%s (count: %d)", cookie->name, cookie->value, cookie_ctx->cookie_count);
    
    // Stream cookie to UART immediately when received
    if (cookie_ctx->send_to_uart) {
        bncurl_cookies_stream_single_to_uart(cookie);
    }
    
    return true;
}

void bncurl_cookies_stream_to_uart(const bncurl_cookie_context_t *cookie_ctx)
{
    if (!cookie_ctx || !cookie_ctx->send_to_uart) {
        return;
    }
    
    ESP_LOGI(TAG, "Streaming %d cookies to UART", cookie_ctx->cookie_count);
    
    for (int i = 0; i < cookie_ctx->cookie_count; i++) {
        const bncurl_cookie_t *cookie = &cookie_ctx->cookies[i];
        bncurl_cookies_stream_single_to_uart(cookie);
    }
}

static void bncurl_cookies_stream_single_to_uart(const bncurl_cookie_t *cookie)
{
    if (!cookie) {
        return;
    }
    
    // Create cookie output line
    char cookie_line[256];
    int line_len = snprintf(cookie_line, sizeof(cookie_line), 
                           "+COOKIE:%s=%s", cookie->name, cookie->value);
    
    // Add domain if available
    if (strlen(cookie->domain) > 0) {
        int remaining = sizeof(cookie_line) - line_len - 1;
        line_len += snprintf(cookie_line + line_len, remaining, "; Domain=%s", cookie->domain);
    }
    
    // Add path if available
    if (strlen(cookie->path) > 0) {
        int remaining = sizeof(cookie_line) - line_len - 1;
        line_len += snprintf(cookie_line + line_len, remaining, "; Path=%s", cookie->path);
    }
    
    // Add flags
    if (cookie->secure) {
        int remaining = sizeof(cookie_line) - line_len - 1;
        line_len += snprintf(cookie_line + line_len, remaining, "; Secure");
    }
    
    if (cookie->http_only) {
        int remaining = sizeof(cookie_line) - line_len - 1;
        line_len += snprintf(cookie_line + line_len, remaining, "; HttpOnly");
    }
    
    // Add CRLF and send
    if (line_len < sizeof(cookie_line) - 2) {
        strcat(cookie_line, "\r\n");
        esp_at_port_write_data((uint8_t *)cookie_line, strlen(cookie_line));
    }
}

bool bncurl_cookies_save_to_file(const bncurl_cookie_context_t *cookie_ctx)
{
    if (!cookie_ctx || !cookie_ctx->save_to_file || strlen(cookie_ctx->save_file_path) == 0) {
        return false;
    }
    
    ESP_LOGI(TAG, "Saving %d cookies to file: %s", cookie_ctx->cookie_count, cookie_ctx->save_file_path);
    
    // Validate and prepare file path
    if (!bncurl_cookies_validate_file_path(cookie_ctx->save_file_path)) {
        ESP_LOGE(TAG, "Failed to validate cookie file path: %s", cookie_ctx->save_file_path);
        return false;
    }
    
    FILE *fp = fopen(cookie_ctx->save_file_path, "w");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to create cookie file: %s", cookie_ctx->save_file_path);
        return false;
    }
    
    // Write Netscape cookie file header
    fprintf(fp, "# Netscape HTTP Cookie File\n");
    fprintf(fp, "# This is a generated file! Do not edit.\n\n");
    
    for (int i = 0; i < cookie_ctx->cookie_count; i++) {
        const bncurl_cookie_t *cookie = &cookie_ctx->cookies[i];
        
        // Write in Netscape cookie format:
        // domain, domain_specified, path, secure, expires, name, value
        fprintf(fp, "%s\t%s\t%s\t%s\t%ld\t%s\t%s\n",
                strlen(cookie->domain) > 0 ? cookie->domain : "localhost",
                "TRUE",  // domain_specified
                strlen(cookie->path) > 0 ? cookie->path : "/",
                cookie->secure ? "TRUE" : "FALSE",
                cookie->expires,
                cookie->name,
                cookie->value);
    }
    
    fclose(fp);
    ESP_LOGI(TAG, "Cookies saved successfully to: %s", cookie_ctx->save_file_path);
    return true;
}

bool bncurl_cookies_validate_file_path(const char *cookie_file_path)
{
    if (!cookie_file_path || strlen(cookie_file_path) == 0) {
        return false;
    }
    
    // Check if this is an SD card path
    if (strncmp(cookie_file_path, "/sdcard", 7) == 0) {
        if (!bnsd_is_mounted()) {
            ESP_LOGE(TAG, "SD card must be mounted to save cookies to: %s", cookie_file_path);
            return false;
        }
    }
    
    // Extract directory path from file path
    char dir_path[BNCURL_MAX_COOKIE_FILE_PATH + 1];
    strncpy(dir_path, cookie_file_path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
    
    // Find the last '/' to separate directory from filename
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL) {
        *last_slash = '\0'; // Terminate string at last slash to get directory path
        
        // Create directory if needed
        if (!bnsd_mkdir_recursive(dir_path)) {
            ESP_LOGE(TAG, "Failed to create directory for cookie file: %s", cookie_file_path);
            return false;
        }
    }
    
    ESP_LOGI(TAG, "Cookie file path validated: %s", cookie_file_path);
    return true;
}

// Libcurl cookie write callback function
int bncurl_cookies_write_callback(const char *cookie, void *userdata)
{
    bncurl_cookie_context_t *cookie_ctx = (bncurl_cookie_context_t *)userdata;
    
    if (!cookie_ctx || !cookie) {
        return CURL_WRITEFUNC_ERROR;
    }
    
    ESP_LOGI(TAG, "Cookie write callback: %s", cookie);
    
    // Parse and add the cookie
    if (bncurl_cookies_parse_and_add(cookie_ctx, cookie)) {
        return 0; // Success
    } else {
        return CURL_WRITEFUNC_ERROR;
    }
}
