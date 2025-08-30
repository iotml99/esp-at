#ifndef BNCURL_H
#define BNCURL_H

#include <stdbool.h>
#include <string.h>
#include "bncurl_params.h"


typedef struct 
{
    bncurl_params_t params;

    /* data */
}bncurl_context_t;


bool bncurl_init(bncurl_context_t *ctx);

#endif /* BNCURL_H */