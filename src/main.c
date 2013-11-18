#include "litestore.h"

#include <stdio.h>
#include <stdlib.h>
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
            char* data;
            size_t len;
            if (litestore_get(ctx, key, strlen(key), &data, &len)
                == LITESTORE_OK)
            {
                printf("GET: %zu bytes, '%s'\n", len, data);
                free(data);
            }
            else
            {
                printf("GET ERR\n");
            }

            const char* key2 = "null_key";
            if (litestore_save(ctx, key2, strlen(key2), NULL, 0)
                == LITESTORE_OK)
            {
                if (litestore_get(ctx, key2, strlen(key2), NULL, NULL)
                    == LITESTORE_OK)
                {
                    printf("GET null value\n");
                }
                else
                {
                    printf("GET ERR\n");
                }
                if (litestore_delete(ctx, key2, strlen(key2))
                    == LITESTORE_OK)
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
