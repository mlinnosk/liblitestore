#include "litestore.h"

#include <stdio.h>


int main()
{
    LiteStoreCtx* ctx = litestore_open("/tmp/foo");
    if (ctx)
    {
        printf("OK\n");
        litestore_close(ctx);
    }
    else
    {
        printf("ERROR\n");
    }

    return 0;
}
