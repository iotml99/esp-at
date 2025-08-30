#ifndef BNCURL_H
#define BNCURL_H

#include <stdbool.h>
#include <string.h>
#include "bncurl_params.h"


typedef struct 
{
    bool is_running;
    uint32_t timeout;
    uint64_t bytes_transferred;
    uint64_t bytes_total;
    bncurl_params_t params;
    /* data */
}bncurl_context_t;


bool bncurl_init(bncurl_context_t *ctx);

uint32_t bncurl_get_timeout(bncurl_context_t *ctx);
bool bncurl_set_timeout(bncurl_context_t *ctx, uint32_t timeout);

bool bncurl_stop(bncurl_context_t *ctx);
bool bncurl_is_running(bncurl_context_t *ctx);

void bncurl_get_progress(bncurl_context_t *ctx, uint64_t *bytes_transferred, uint64_t *bytes_total);

#endif /* BNCURL_H */