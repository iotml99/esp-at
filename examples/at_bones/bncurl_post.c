/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bncurl_post.h"
#include "bncurl_common.h"
#include "bncurl_methods.h"
#include "bncurl_config.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_at.h"
#include "esp_log.h"

static const char *TAG = "BNCURL_POST";

// Structure to capture content length from HEAD request
typedef struct {
    size_t content_length;
    bool found;
} head_response_t;

// Header callback for HEAD request to extract content length
static size_t head_header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    head_response_t *head_response = (head_response_t *)userdata;
    size_t total_size = size * nitems;
    
    // Look for Content-Length header
    if (strncasecmp(buffer, "Content-Length:", 15) == 0) {
        char *length_str = buffer + 15;
        while (*length_str == ' ' || *length_str == '\t') length_str++; // Skip whitespace
        
        head_response->content_length = strtoul(length_str, NULL, 10);
        head_response->found = true;
        ESP_LOGI(TAG, "HEAD request detected Content-Length: %u bytes", (unsigned int)head_response->content_length);
    }
    
    return total_size;
}

// Function to perform HEAD request and get content length
static bool get_content_length_via_head(bncurl_context_t *ctx, size_t *content_length)
{
    CURL *curl;
    CURLcode res;
    head_response_t head_response = {0, false};
    bool success = false;
    
    *content_length = 0; // Initialize to 0
    
    // Initialize curl for HEAD request
    curl = curl_easy_init();
    if (!curl) {
        ESP_LOGE(TAG, "Failed to initialize curl for HEAD request");
        return false;
    }
    
    ESP_LOGI(TAG, "Making HEAD request to get content length: %s", ctx->params.url);
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, ctx->params.url);
    
    // Configure as HEAD request
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    
    // Set timeout (shorter timeout for HEAD request)
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    
    // Set header callback to capture Content-Length
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, head_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &head_response);
    
    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, BNCURL_MAX_REDIRECTS);
    
    // Set User-Agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, BNCURL_DEFAULT_USER_AGENT);
    
    // Configure DNS and connection settings
    curl_easy_setopt(curl, CURLOPT_DNS_SERVERS, "8.8.8.8,1.1.1.1,208.67.222.222");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    
    // Configure HTTPS/TLS settings (same as in bncurl_common.c)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_DEFAULT);
    
    // Add custom headers if provided (but skip Content-Length related headers for HEAD)
    struct curl_slist *headers = NULL;
    if (ctx->params.header_count > 0) {
        for (int i = 0; i < ctx->params.header_count; i++) {
            // Skip content-type and content-length headers for HEAD request
            if (strncasecmp(ctx->params.headers[i], "Content-Type:", 13) != 0 &&
                strncasecmp(ctx->params.headers[i], "Content-Length:", 15) != 0) {
                headers = curl_slist_append(headers, ctx->params.headers[i]);
            }
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    // Perform the HEAD request
    res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        // Get response code
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        if (response_code >= 200 && response_code < 300) {
            if (head_response.found) {
                *content_length = head_response.content_length;
                success = true;
                ESP_LOGI(TAG, "HEAD request successful, Content-Length: %u bytes", (unsigned int)*content_length);
            } else {
                ESP_LOGW(TAG, "HEAD request successful but no Content-Length header found");
                // Not an error - some servers don't provide Content-Length
                success = false;
            }
        } else {
            ESP_LOGW(TAG, "HEAD request failed with HTTP code: %ld", response_code);
        }
    } else {
        ESP_LOGW(TAG, "HEAD request curl error: %s (code: %d)", curl_easy_strerror(res), res);
    }
    
    // Cleanup
    if (headers) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);
    
    return success;
}

bool bncurl_execute_post_request(bncurl_context_t *ctx)
{
    if (!ctx) {
        ESP_LOGE(TAG, "Invalid context");
        return false;
    }
    
    ESP_LOGI(TAG, "Starting POST request to: %s", ctx->params.url);
    
    // First, make a HEAD request to get the content length
    size_t expected_content_length = 0;
    bool has_content_length = get_content_length_via_head(ctx, &expected_content_length);
    
    // Output +LEN: marker if content length was determined
    if (has_content_length && expected_content_length > 0) {
        char len_marker[64];
        snprintf(len_marker, sizeof(len_marker), "+LEN:%u\r\n", (unsigned int)expected_content_length);
        esp_at_port_write_data((uint8_t *)len_marker, strlen(len_marker));
        ESP_LOGI(TAG, "Sent +LEN marker: %u bytes expected", (unsigned int)expected_content_length);
    } else if (!has_content_length) {
        ESP_LOGW(TAG, "Could not determine content length via HEAD request, proceeding without +LEN marker");
    } else {
        ESP_LOGI(TAG, "Content length is 0, no +LEN marker needed");
    }
    
    // Initialize streaming context
    bncurl_stream_context_t stream;
    bncurl_stream_init(&stream, ctx);
    
    // Set expected content length if we have it
    if (has_content_length) {
        stream.total_size = expected_content_length;
        ctx->bytes_total = expected_content_length;
    }
    
    // Use common functionality for POST request
    bool success = bncurl_common_execute_request(ctx, &stream, "POST");
    
    // Finalize streaming
    bncurl_stream_finalize(&stream, success);
    
    return success;
}
