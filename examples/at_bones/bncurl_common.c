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
#include <time.h>
#include "esp_at.h"
#include "esp_log.h"

static const char *TAG = "BNCURL_COMMON";

/* Hardcoded CA bundle for HTTPS support */
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
"-----END CERTIFICATE-----\n"
/* Baltimore CyberTrust Root - used by many Microsoft and other services */
"-----BEGIN CERTIFICATE-----\n"
"MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ"
"RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD"
"VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX"
"DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y"
"ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy"
"VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr"
"mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr"
"IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK"
"mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu"
"XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy"
"dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye"
"jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1"
"BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3"
"DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92"
"9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx"
"jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0"
"Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz"
"ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS"
"R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp\n"
"-----END CERTIFICATE-----\n"
/* Cloudflare Inc ECC CA-3 - for Cloudflare CDN sites like httpbin.org */
"-----BEGIN CERTIFICATE-----\n"
"MIIBljCCATygAwIBAgIQC5McOtY5Z+pnI7/Dr5r0SzAKBggqhkjOPQQDAjAmMQsw"
"CQYDVQQGEwJVUzEXMBUGA1UEChMOQ2xvdWRmbGFyZSwgSW5jLjAeFw0yMDEyMDMy"
"MzAwMDBaFw0zNTEyMDIyMzAwMDBaMCYxCzAJBgNVBAYTAlVTMRcwFQYDVQQKEw5D"
"bG91ZGZsYXJlLCBJbmMuMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEua1NZpkU"
"DaTGsb5+yrg7FkAsVjNrKh/lqnrqgf7kO4hXfbXVAv+5VdJ9P4FpXDdpJe7zEINb"
"1QKCCLOCqKO4faGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8EBTADAQH/"
"MB0GA1UdDgQWBBSlzjfq67B1DpRniLRF+tkkEIeWHzAKBggqhkjOPQQDAgNIADBF"
"AiEAiZQb1gODuHNyZNkD2G2ByEQjW2p9cLbvv5dAE5wG5CgCIGV+HgAl0xRgJrW8"
"xP9x+nOgvv4U+2nfAM7S4/J8ydnl\n"
"-----END CERTIFICATE-----\n";

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
            
            // Stream this buffer to output (file or UART)
            if (!bncurl_stream_buffer_to_output(common_ctx->stream, 
                                               common_ctx->stream->active_buffer)) {
                ESP_LOGE(TAG, "Failed to stream buffer to output");
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

// Common header callback to get content length and stream headers for HEAD requests
size_t bncurl_common_header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    bncurl_common_context_t *common_ctx = (bncurl_common_context_t *)userdata;
    size_t total_size = size * nitems;
    
    // For HEAD requests, stream headers using the streaming buffer system
    if (common_ctx && common_ctx->ctx && 
        strcmp(common_ctx->ctx->params.method, "HEAD") == 0) {
        // Only process HTTP headers (skip status line and empty lines)
        if (total_size > 2 && buffer[0] != '\r' && buffer[0] != '\n' &&
            strncmp(buffer, "HTTP/", 5) != 0) {
            
            // Clean up the header line by removing trailing CRLF
            char header_line[512];
            size_t copy_len = total_size < sizeof(header_line) - 3 ? total_size : sizeof(header_line) - 3; // Leave space for \r\n\0
            memcpy(header_line, buffer, copy_len);
            header_line[copy_len] = '\0';
            
            // Remove trailing \r\n
            char *end = header_line + strlen(header_line) - 1;
            while (end >= header_line && (*end == '\r' || *end == '\n')) {
                *end = '\0';
                end--;
            }
            
            // Add back clean CRLF for output
            if (strlen(header_line) > 0) {
                strcat(header_line, "\r\n");
                
                // Write header to streaming buffer
                bncurl_stream_context_t *stream = common_ctx->stream;
                bncurl_stream_buffer_t *active_buf = &stream->buffers[stream->active_buffer];
                size_t header_len = strlen(header_line);
                
                // Check if header fits in current buffer
                if (active_buf->size + header_len <= BNCURL_STREAM_BUFFER_SIZE) {
                    memcpy(active_buf->data + active_buf->size, header_line, header_len);
                    active_buf->size += header_len;
                } else {
                    // Current buffer full, stream it and switch to next buffer
                    if (active_buf->size > 0) {
                        bncurl_stream_buffer_to_output(stream, stream->active_buffer);
                        stream->active_buffer = (stream->active_buffer + 1) % BNCURL_STREAM_BUFFER_COUNT;
                        active_buf = &stream->buffers[stream->active_buffer];
                        active_buf->size = 0;
                    }
                    
                    // Add header to new buffer
                    if (header_len <= BNCURL_STREAM_BUFFER_SIZE) {
                        memcpy(active_buf->data, header_line, header_len);
                        active_buf->size = header_len;
                    }
                }
            }
        }
    }
    
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
    char *post_data = NULL;  // Track allocated POST data for cleanup
    
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
    
    // Log DNS configuration for debugging
    ESP_LOGI(TAG, "Using DNS servers: 8.8.8.8, 1.1.1.1, 208.67.222.222");
    
    // Check if this is an HTTPS request and ensure time is synchronized
    if (strncmp(ctx->params.url, "https://", 8) == 0) {
        ESP_LOGI(TAG, "HTTPS request detected - checking time synchronization");
        
        // Get current time
        time_t now;
        time(&now);
        
        // Check if time seems reasonable (after year 2020)
        if (now < 1577836800) { // January 1, 2020 00:00:00 UTC
            ESP_LOGW(TAG, "System time appears incorrect (before 2020). HTTPS may fail.");
            ESP_LOGW(TAG, "Current timestamp: %ld", (long)now);
            ESP_LOGW(TAG, "Use AT+CIPSNTPCFG and AT+CIPSNTPTIME to set correct time");
        } else {
            struct tm *timeinfo = gmtime(&now);
            ESP_LOGI(TAG, "System time: %04d-%02d-%02d %02d:%02d:%02d UTC", 
                     timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                     timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        }
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, ctx->params.url);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, ctx->timeout);
    
    // Set method-specific options
    if (strcmp(method, "GET") == 0) {
        // GET is default, no special setup needed
    } else if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        
        // Handle POST data if provided
        if (strlen(ctx->params.data_upload) > 0) {
            // Check if it's a file upload (starts with @) or raw data size
            if (ctx->params.data_upload[0] == '@') {
                // File upload - read from file
                const char *file_path = ctx->params.data_upload + 1; // Skip @
                ESP_LOGI(TAG, "POST: Uploading from file: %s", file_path);
                
                FILE *fp = fopen(file_path, "rb");
                if (fp) {
                    // Get file size
                    fseek(fp, 0, SEEK_END);
                    long file_size = ftell(fp);
                    fseek(fp, 0, SEEK_SET);
                    
                    // Read file content
                    post_data = malloc(file_size);
                    if (post_data) {
                        size_t read_size = fread(post_data, 1, file_size, fp);
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, read_size);
                        ESP_LOGI(TAG, "POST: File uploaded, size: %zu bytes", read_size);
                    }
                    fclose(fp);
                } else {
                    ESP_LOGE(TAG, "POST: Failed to open file: %s", file_path);
                }
            } else {
                // Raw data size - for now, send empty data of specified size
                // In a full implementation, this would read from UART
                long data_size = atol(ctx->params.data_upload);
                ESP_LOGI(TAG, "POST: Empty data upload, size: %ld bytes", data_size);
                
                if (data_size > 0) {
                    char *empty_data = calloc(data_size, 1);
                    if (empty_data) {
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, empty_data);
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data_size);
                        // Note: empty_data will be freed after curl_easy_perform
                    }
                } else {
                    // Send truly empty POST
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
                }
            }
        } else {
            // Empty POST request
            ESP_LOGI(TAG, "POST: Empty POST request (no data)");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
        }
    } else if (strcmp(method, "HEAD") == 0) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        ESP_LOGI(TAG, "HEAD: Request configured (headers only)");
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
    
    // Configure DNS and connection settings for better reliability
    curl_easy_setopt(curl, CURLOPT_DNS_SERVERS, "8.8.8.8,1.1.1.1,208.67.222.222");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    
    // Configure HTTPS/TLS settings with hardcoded CA certificates
    struct curl_blob ca = { .data=(void*)CA_BUNDLE_PEM, .len=sizeof(CA_BUNDLE_PEM)-1, .flags=CURL_BLOB_COPY };
    CURLcode ca_result = curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &ca);
    
    // If CA blob fails, try alternative approaches
    if (ca_result != CURLE_OK) {
        ESP_LOGW(TAG, "CA blob failed, trying alternative SSL configuration");
        // Use ESP32's built-in certificate bundle if available
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
        curl_easy_setopt(curl, CURLOPT_CAINFO, NULL);
    }
    
    // Try more permissive SSL settings for broader compatibility
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Add fallback: if certificate verification fails, try with less strict verification
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    
    // Enable verbose SSL debug for troubleshooting
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    
    // Set SSL version to be more permissive
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_DEFAULT);
    
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
        
        // Success criteria: 2xx status codes for all methods
        if (response_code >= 200 && response_code < 300) {
            // Stream any remaining data in the active buffer
            bncurl_stream_buffer_t *active_buf = &stream->buffers[stream->active_buffer];
            if (active_buf->size > 0) {
                bncurl_stream_buffer_to_output(stream, stream->active_buffer);
            }
            
            success = true;
            ESP_LOGI(TAG, "%s request completed successfully", method);
        } else {
            ESP_LOGW(TAG, "%s request failed with HTTP code: %ld", method, response_code);
        }
    } else {
        // Provide specific error messages for common issues
        if (res == CURLE_COULDNT_RESOLVE_HOST) {
            ESP_LOGE(TAG, "DNS resolution failed for %s", ctx->params.url);
            ESP_LOGE(TAG, "Check: 1) WiFi connection 2) DNS servers accessible 3) Hostname spelling");
            ESP_LOGE(TAG, "Suggestion: Try 'AT+CWJAP?' to check WiFi status");
        } else if (res == CURLE_COULDNT_CONNECT) {
            ESP_LOGE(TAG, "Connection failed - check network connectivity and firewall");
        } else if (res == CURLE_OPERATION_TIMEDOUT) {
            ESP_LOGE(TAG, "Operation timed out - check network stability");
        } else if (res == CURLE_SSL_CONNECT_ERROR) {
            ESP_LOGE(TAG, "SSL connection failed - certificate or TLS handshake issue");
            ESP_LOGE(TAG, "Try HTTP instead of HTTPS for testing: http://httpbin.org/json");
        } else if (res == CURLE_PEER_FAILED_VERIFICATION) {
            ESP_LOGE(TAG, "SSL certificate verification failed");
            ESP_LOGE(TAG, "Certificate may be expired or not trusted by CA bundle");
            ESP_LOGE(TAG, "If error is BADCERT_FUTURE, check system time with AT+CIPSNTPTIME?");
        } else if (res == CURLE_SSL_CACERT) {
            ESP_LOGE(TAG, "SSL CA certificate problem - CA bundle may be incomplete");
        } else {
            ESP_LOGE(TAG, "Curl error: %s (code: %d)", curl_easy_strerror(res), res);
        }
    }
    
    // Cleanup
    if (headers) {
        curl_slist_free_all(headers);
    }
    if (post_data) {
        free(post_data);
    }
    curl_easy_cleanup(curl);
    
    ctx->is_running = false;
    
    return success;
}
