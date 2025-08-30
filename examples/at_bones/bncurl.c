#include "bncurl.h"

bool bncurl_init(bncurl_context_t *ctx)
{
    if (!ctx)
    {
        return false;
    }

    // Initialize the BNCURL context
    memset(ctx, 0, sizeof(bncurl_context_t));
    return true;
}
