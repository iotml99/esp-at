/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bncurl_common.h"
#include "bncurl_methods.h"
#include "bncurl_config.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_at.h"
#include "esp_log.h"

static const char *TAG = "BNCURL_COMMON";

// Common write callback for streaming with dual-buffer
size_t bncurl_common_write_callback(void *contents, size_t size, size_t nmemb, void *userdata)
{
    bncurl_common_context_t *common_ctx = (bncurl_common_context_t *)userdata;
    size_t total_size = size * nmemb;
    size_t bytes_written = 0;
    
    // Check if operation should be stopped
    if (!common_ctx->ctx->is_running) {
        return 0; // Abort the transfer
    }
    
    while (bytes_written < total_size) {
        bncurl_stream_buffer_t *active_buf = &common_ctx->stream->buffers[common_ctx->stream->active_buffer];
        size_t remaining_in_buffer = BNCURL_STREAM_BUFFER_SIZE - active_buf->size;
        size_t remaining_data = total_size - bytes_written;
        size_t bytes_to_copy = (remaining_in_buffer < remaining_data) ? remaining_in_buffer : remaining_data;
        
        // Copy data to active buffer
        memcpy(active_buf->data + active_buf->size, 
               (char *)contents + bytes_written, 
               bytes_to_copy);
        active_buf->size += bytes_to_copy;
        bytes_written += bytes_to_copy;
        
        // Check if buffer is full
        if (active_buf->size >= BNCURL_STREAM_BUFFER_SIZE) {
            active_buf->is_full = true;
            
            // Stream this buffer to UART
            if (!bncurl_stream_buffer_to_uart(common_ctx->stream, common_ctx->stream->active_buffer)) {
                ESP_LOGE(TAG, "Failed to stream buffer to UART");
                return 0; // Abort on error
            }
            
            // Switch to the other buffer
            common_ctx->stream->active_buffer = (common_ctx->stream->active_buffer + 1) % BNCURL_STREAM_BUFFER_COUNT;
            bncurl_stream_buffer_t *new_buf = &common_ctx->stream->buffers[common_ctx->stream->active_buffer];
            new_buf->size = 0;
            new_buf->is_full = false;
            new_buf->is_streaming = false;
        }
        
        // Update progress
        common_ctx->ctx->bytes_transferred += bytes_to_copy;
    }
    
    return total_size;
}

// Common header callback to get content length
size_t bncurl_common_header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    bncurl_common_context_t *common_ctx = (bncurl_common_context_t *)userdata;
    size_t total_size = size * nitems;
    
    // Look for Content-Length header
    if (strncasecmp(buffer, "Content-Length:", 15) == 0) {
        char *length_str = buffer + 15;
        while (*length_str == ' ' || *length_str == '\t') length_str++; // Skip whitespace
        
        common_ctx->stream->total_size = strtoul(length_str, NULL, 10);
        common_ctx->ctx->bytes_total = common_ctx->stream->total_size;
        ESP_LOGI(TAG, "Content-Length detected: %u bytes", (unsigned int)common_ctx->stream->total_size);
    }
    
    return total_size;
}

// Common progress callback
int bncurl_common_progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                   curl_off_t ultotal, curl_off_t ulnow)
{
    bncurl_common_context_t *common_ctx = (bncurl_common_context_t *)clientp;
    
    if (common_ctx && common_ctx->ctx) {
        // Update total if we didn't get it from headers
        if (dltotal > 0 && common_ctx->stream->total_size == 0) {
            common_ctx->stream->total_size = (size_t)dltotal;
            common_ctx->ctx->bytes_total = common_ctx->stream->total_size;
        }
        
        // Check if operation should be stopped
        if (!common_ctx->ctx->is_running) {
            return 1; // Abort the transfer
        }
    }
    
    return 0; // Continue
}

bool bncurl_common_execute_request(bncurl_context_t *ctx, bncurl_stream_context_t *stream, 
                                   const char *method)
{
    if (!ctx || !stream || !method) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }
    
    CURL *curl;
    CURLcode res;
    bool success = false;
    
    // Initialize common context
    bncurl_common_context_t common_ctx;
    common_ctx.ctx = ctx;
    common_ctx.stream = stream;
    
    // Initialize curl
    curl = curl_easy_init();
    if (!curl) {
        ESP_LOGE(TAG, "Failed to initialize curl");
        return false;
    }
    
    ctx->is_running = true;
    ctx->bytes_transferred = 0;
    ctx->bytes_total = 0;
    
    ESP_LOGI(TAG, "Starting %s request to: %s", method, ctx->params.url);
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, ctx->params.url);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, ctx->timeout);
    
    // Set method-specific options
    if (strcmp(method, "GET") == 0) {
        // GET is default, no special setup needed
    } else if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        // TODO: Add POST data handling
    } else if (strcmp(method, "HEAD") == 0) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }
    
    // Set callbacks for data receiving (not for HEAD requests)
    if (strcmp(method, "HEAD") != 0) {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, bncurl_common_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &common_ctx);
    }
    
    // Set header callback
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, bncurl_common_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &common_ctx);
    
    // Set progress callback
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, bncurl_common_progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &common_ctx);
    
    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, BNCURL_MAX_REDIRECTS);
    
    // Set User-Agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, BNCURL_DEFAULT_USER_AGENT);
    
    // Add custom headers if provided
    struct curl_slist *headers = NULL;
    if (ctx->params.header_count > 0) {
        for (int i = 0; i < ctx->params.header_count; i++) {
            headers = curl_slist_append(headers, ctx->params.headers[i]);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    // Perform the request
    res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        // Get response code
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        if (response_code == 200 || (strcmp(method, "HEAD") == 0 && response_code == 200)) {
            // For non-HEAD requests, stream any remaining data in the active buffer
            if (strcmp(method, "HEAD") != 0) {
                bncurl_stream_buffer_t *active_buf = &stream->buffers[stream->active_buffer];
                if (active_buf->size > 0) {
                    bncurl_stream_buffer_to_uart(stream, stream->active_buffer);
                }
            }
            
            success = true;
            ESP_LOGI(TAG, "%s request completed successfully", method);
        } else {
            ESP_LOGW(TAG, "%s request failed with HTTP code: %ld", method, response_code);
        }
    } else {
        ESP_LOGE(TAG, "Curl error: %s", curl_easy_strerror(res));
    }
    
    // Cleanup
    if (headers) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);
    
    ctx->is_running = false;
    
    return success;
}
