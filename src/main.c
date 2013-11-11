#include "litestore.h"

#include <stdio.h>


int main()
{
    litestore_ctx* ctx = litestore_open("/tmp/foo");
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
