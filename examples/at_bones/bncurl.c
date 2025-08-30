#include "bncurl.h"
#include "bncurl_config.h"

bool bncurl_init(bncurl_context_t *ctx)
{
    if (!ctx)
    {
        return false;
    }

    // Initialize the BNCURL context
    memset(ctx, 0, sizeof(bncurl_context_t));
    
    // Set default timeout value
    ctx->timeout = BNCURL_DEFAULT_TIMEOUT;
    
    return true;
}

uint32_t bncurl_get_timeout(bncurl_context_t *ctx)
{
    if (!ctx)
    {
        return 0;
    }
    return ctx->timeout;
}

bool bncurl_set_timeout(bncurl_context_t *ctx, uint32_t timeout)
{
    if (!ctx)
    {
        return false;
    }
    if(timeout < BNCURL_MIN_TIMEOUT || timeout > BNCURL_MAX_TIMEOUT)
    {
        return false;
    }
    ctx->timeout = timeout;
    return true;
}
