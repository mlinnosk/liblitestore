#include "litestore.h"

#include <stdio.h>


int main()
{
    litestore_ctx* ctx = NULL;
    if (litestore_open("/tmp/foo.db", &ctx) == LITESTORE_OK)
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
