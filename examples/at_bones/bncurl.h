#ifndef BNCURL_H
#define BNCURL_H

#include <stdbool.h>
#include <string.h>
#include "bncurl_params.h"

// Forward declarations
typedef struct bncurl_response bncurl_response_t;

typedef struct 
{
    bool is_running;
    uint32_t timeout;
    uint64_t bytes_transferred;
    uint64_t bytes_total;
    bncurl_params_t params;
    
    // Certificate data pointers for cleanup (allocated by certificate manager)
    uint8_t *ca_cert_data;
    uint8_t *client_cert_data;
    uint8_t *client_key_data;
    /* data */
}bncurl_context_t;

// Response structure for HTTP requests
typedef struct bncurl_response {
    char *data;
    size_t size;
    size_t capacity;
    long response_code;
    char *content_type;
} bncurl_response_t;

bool bncurl_init(bncurl_context_t *ctx);

/**
 * @brief Get current server response timeout value
 * @param ctx BNCURL context
 * @return Timeout in seconds (server inactivity timeout)
 */
uint32_t bncurl_get_timeout(bncurl_context_t *ctx);

/**
 * @brief Set server response timeout
 * @param ctx BNCURL context  
 * @param timeout Server response timeout in seconds (1-120)
 *                If no data received from server for this duration, connection is closed
 * @return true on success, false on error
 */
bool bncurl_set_timeout(bncurl_context_t *ctx, uint32_t timeout);

bool bncurl_stop(bncurl_context_t *ctx);
bool bncurl_is_running(bncurl_context_t *ctx);

void bncurl_cleanup_certificates(bncurl_context_t *ctx);

void bncurl_get_progress(bncurl_context_t *ctx, uint64_t *bytes_transferred, uint64_t *bytes_total);

// Forward declarations for method-specific executors
// These are implemented in separate files: bncurl_get.c, bncurl_post.c, bncurl_head.c
bool bncurl_execute_get_request(bncurl_context_t *ctx);
bool bncurl_execute_post_request(bncurl_context_t *ctx);
bool bncurl_execute_head_request(bncurl_context_t *ctx);

// Helper functions for response management (deprecated - use streaming approach)
bool bncurl_response_init(bncurl_response_t *response);
void bncurl_response_cleanup(bncurl_response_t *response);

#endif /* BNCURL_H */