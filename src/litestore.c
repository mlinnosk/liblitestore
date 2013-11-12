#include "litestore.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    sqlite3_stmt* save_stmt;
};

int prepare_statements(litestore_ctx* ctx)
{
    const char* save = "INSERT INTO objects (name, type) VALUES (?, ?);";
    if (sqlite3_prepare_v2(ctx->db,
                           save,
                           -1,
                           &(ctx->save_stmt),
                           NULL)
        != SQLITE_OK)
    {
        printf("ERROR: %s\n", sqlite3_errmsg(ctx->db));
        return LITESTORE_ERR;
    }
    return LITESTORE_OK;
}

int finalize_statements(litestore_ctx* ctx)
{
    sqlite3_finalize(ctx->save_stmt);
    ctx->save_stmt = NULL;
    return LITESTORE_OK;
}

int litestore_open(const char* db_file_name, litestore_ctx** ctx)
{
    *ctx = (litestore_ctx*)malloc(sizeof(litestore_ctx));
    if (*ctx)
    {
        memset(*ctx, 0, sizeof(litestore_ctx));

        if (sqlite3_open(db_file_name, &(*ctx)->db) != SQLITE_OK)
        {
            litestore_close(*ctx);
            *ctx = NULL;
            return LITESTORE_ERR;
        }

        return prepare_statements(*ctx);
    }
    return LITESTORE_ERR;
}

void litestore_close(litestore_ctx* ctx)
{
    if (ctx)
    {
        if (ctx->db)
        {
            finalize_statements(ctx);
            sqlite3_close(ctx->db);
            ctx->db = NULL;
        }
        free(ctx);
    }
}

int litestore_get(litestore_ctx* ctx, const char* key, char** value)
{
    return LITESTORE_OK;
}

int litestore_save(litestore_ctx* ctx,
                   const char* key, const char* value)
{
    return LITESTORE_OK;
}

#ifdef __cplusplus
}  // extern "C"
#endif
