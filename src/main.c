#include "litestore.h"

#include <stdio.h>
#include <string.h>


int main()
{
    litestore_ctx* ctx = NULL;
    if (litestore_open("/tmp/foo.db", &ctx) == LITESTORE_OK)
    {
        const char* key = "foo";
        const char* value = "value";
        if (litestore_save(ctx, key, strlen(key), value, strlen(value))
            == LITESTORE_OK)
        {
            printf("OK\n");
        }
        litestore_close(ctx);
    }
    else
    {
        printf("ERROR\n");
    }

    return 0;
}
