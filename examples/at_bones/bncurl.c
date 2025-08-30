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


bool bncurl_is_running(bncurl_context_t *ctx)
{
    if (!ctx)
    {
        return false;
    }
    return ctx->is_running;
}

bool bncurl_stop(bncurl_context_t *ctx)
{
    if (!ctx)
    {
        return false;
    }
    ctx->is_running = false;
    return true ;
}

void bncurl_get_progress(bncurl_context_t *ctx, uint64_t *bytes_transferred, uint64_t *bytes_total)
{
    if (!ctx || !bytes_transferred || !bytes_total)
    {
        if (bytes_transferred) *bytes_transferred = 0;
        if (bytes_total) *bytes_total = 0;
        return;
    }
    
    *bytes_transferred = ctx->bytes_transferred;
    *bytes_total = ctx->bytes_total;
}