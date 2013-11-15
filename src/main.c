#include "litestore.h"

#include <stdio.h>
#include <string.h>


int main()
{
    litestore* ctx = NULL;
    if (litestore_open("/tmp/foo.db", &ctx) == LITESTORE_OK)
    {
        const char* key = "foo";
        const char* value = "value";
        if (litestore_save(ctx, key, strlen(key), value, strlen(value))
            == LITESTORE_OK)
        {
            printf("SAVE OK\n");
            if (litestore_delete(ctx, key, strlen(key)) == LITESTORE_OK)
            {
                printf("DEL OK\n");
            }
            else
            {
                printf("DEL ERR\n");
            }
        }
        else
        {
            printf("SAVE ERR\n");
        }
        litestore_close(ctx);
    }
    else
    {
        printf("ERROR\n");
    }

    return 0;
}
