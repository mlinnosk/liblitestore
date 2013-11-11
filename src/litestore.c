#include "litestore.h"

#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#ifdef __cplusplus
extern "C"
#endif

char* ls_strdup(const char* str)
{
    const size_t LEN = strlen(str) + 1;
    char* copy = (char*)malloc(LEN);
    if (!copy)
    {
        return NULL;
    }
    memcpy(copy, str, LEN - 1);
    copy[LEN] = '\0';
    return copy;
}

struct litestore_ctx
{
    sqlite3* db;
};

litestore_ctx* litestore_open(const char* db_file_name)
{
    litestore_ctx* ctx = (litestore_ctx*)malloc(sizeof(litestore_ctx));
    if (ctx)
    {
        memset(ctx, 0, sizeof(ctx));

        if (sqlite3_open(db_file_name, &ctx->db) != SQLITE_OK)
        {
            litestore_close(ctx);
            ctx = NULL;
        }
    }
    return ctx;
}

void litestore_close(litestore_ctx* ctx)
{
    if (ctx)
    {
        if (ctx->db)
        {
            sqlite3_close(ctx->db);
        }
        free(ctx);
    }
}

char* litestore_get(litestore_ctx* ctx, const char* key)
{
    return NULL;
}

int litestore_save(litestore_ctx* ctx,
                   const char* key, const char* value)
{
    return 0;
}

#ifdef __cplusplus
}  // extern "C"
#endif
