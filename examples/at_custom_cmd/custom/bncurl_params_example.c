/*
 * BNCURL Parameters Usage Example
 * 
 * This file demonstrates how to use the bncurl_params_parse function
 * to parse AT+BNCURL command parameters.
 */

#include "bncurl_params.h"

/* Example usage in AT command handler */
static uint8_t at_bncurl_cmd(uint8_t para_num)
{
    bncurl_params_t params;
    
    /* Parse parameters */
    uint8_t result = bncurl_params_parse(para_num, &params);
    if (result != ESP_AT_RESULT_CODE_OK) {
        return result;
    }
    
    /* Example: Access parsed parameters */
    
    /* Basic parameters */
    switch (params.method) {
        case BNCURL_GET:
            // Handle GET request
            break;
        case BNCURL_POST:
            // Handle POST request with params.upload_* fields
            break;
        case BNCURL_HEAD:
            // Handle HEAD request
            break;
    }
    
    /* URL */
    char *url = params.url;
    
    /* File operations */
    if (params.save_to_file) {
        // Save response to params.save_path
    }
    
    if (params.has_upload) {
        if (params.upload_from_file) {
            // Upload from file: params.upload_path
        } else {
            // Upload from UART: params.upload_size bytes
        }
    }
    
    /* Custom headers */
    for (int i = 0; i < params.header_count; i++) {
        char *header = params.headers_list[i];
        // Add header to request
    }
    
    /* Cookie operations */
    if (params.save_cookies) {
        for (int i = 0; i < params.cookie_save_count; i++) {
            char *save_path = params.cookie_save_paths[i];
            // Setup cookie save to file
        }
    }
    
    if (params.load_cookies) {
        for (int i = 0; i < params.cookie_load_count; i++) {
            char *load_path = params.cookie_load_paths[i];
            // Setup cookie load from file
        }
    }
    
    /* Range request */
    if (params.has_range) {
        uint64_t start = params.range_start;
        uint64_t end = params.range_end;
        // Setup range request: start-end
    }
    
    /* Verbose mode */
    if (params.verbose) {
        // Enable verbose output
    }
    
    return ESP_AT_RESULT_CODE_OK;
}

/*
 * Example AT commands that would be parsed:
 * 
 * Basic GET:
 * AT+BNCURL="GET","https://httpbin.org/get"
 * 
 * GET with file download:
 * AT+BNCURL="GET","https://httpbin.org/json","-dd","/sdcard/response.json"
 * 
 * POST with UART upload:
 * AT+BNCURL="POST","https://httpbin.org/post","-du","100"
 * 
 * POST with file upload and custom headers:
 * AT+BNCURL="POST","https://httpbin.org/post","-du","@/sdcard/data.txt","-H","Content-Type: text/plain","-H","Authorization: Bearer token123"
 * 
 * GET with range download:
 * AT+BNCURL="GET","https://httpbin.org/bytes/1024","-r","0-511","-dd","/sdcard/partial.bin"
 * 
 * GET with cookies:
 * AT+BNCURL="GET","https://httpbin.org/cookies","-b","/sdcard/session.cookies","-c","/sdcard/new_session.cookies"
 * 
 * Complex example with all parameters:
 * AT+BNCURL="POST","https://api.example.com/upload","-du","@/sdcard/payload.json","-dd","/sdcard/response.json","-H","Content-Type: application/json","-H","Authorization: Bearer abc123","-c","/sdcard/session.cookies","-b","/sdcard/auth.cookies","-v"
 */
