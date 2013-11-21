#include "litestore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void getAndPrint(litestore* ctx, const char* key)
{
    char* data = NULL;
    size_t len = 0;
    const int rv = litestore_get(ctx, key, strlen(key), &data, &len);
    if (rv == LITESTORE_OK)
    {
        if (data && len)
        {
            printf("GET: %zu bytes, '%s'\n", len, data);
        }
        else
        {
            printf("GET: NULL data\n");
        }
        free(data);
    }
    else if (rv == LITESTORE_UNKNOWN_ENTITY)
    {
        printf("GET UNKNOWN (%d): %s\n", rv, key);
    }
    else
    {
        printf("GET ERR (%d): %s\n", rv, key);
    }
}

int main()
{
    litestore* ctx = NULL;
    if (litestore_open("/tmp/foo.db", &ctx) == LITESTORE_OK)
    {
        const char* key = "foo";
        const char* value = "value";
        const char* value2 = "value2";
        if (litestore_put(ctx, key, strlen(key), value, strlen(value))
            == LITESTORE_OK)
        {
            printf("SAVE OK\n");
            getAndPrint(ctx, key);

            /* change type to null */
            if (litestore_update(ctx, key, strlen(key), NULL, 0)
                == LITESTORE_OK)
            {
                printf("Update OK\n");
            }
            else
            {
                printf("Update ERR\n");
            }
            getAndPrint(ctx, key);

            if (litestore_update(ctx, key, strlen(key),
                                 value2, strlen(value2)) == LITESTORE_OK)
            {
                printf("Update OK\n");
            }
            else
            {
                printf("Update ERR\n");
            }
            getAndPrint(ctx, key);

            const char* key2 = "null_key";
            if (litestore_put(ctx, key2, strlen(key2), NULL, 0)
                == LITESTORE_OK)
            {
                getAndPrint(ctx, key2);
                if (litestore_update(ctx, key2, strlen(key2),
                                     value2, strlen(value2))
                    == LITESTORE_OK)
                {
                    printf("Update OK\n");
                }
                else
                {
                    printf("Update ERR\n");
                }
                getAndPrint(ctx, key);

                if (litestore_delete(ctx, key2, strlen(key2))
                    == LITESTORE_OK)
                {
                    printf("DEL OK\n");
                }
                else
                {
                    printf("DEL ERR\n");
                }
                getAndPrint(ctx, key2);
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
