#ifndef BNCURL_H
#define BNCURL_H

#include <stdbool.h>
#include <string.h>
#include "bncurl_params.h"


typedef struct 
{
    uint32_t timeout;
    bncurl_params_t params;
    /* data */
}bncurl_context_t;


bool bncurl_init(bncurl_context_t *ctx);

uint32_t bncurl_get_timeout(bncurl_context_t *ctx);
bool bncurl_set_timeout(bncurl_context_t *ctx, uint32_t timeout);

#endif /* BNCURL_H */