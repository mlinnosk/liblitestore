#include "litestore.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3.h>

#ifdef __cplusplus
extern "C"
#endif

/**
 * The LiteStore object.
 */
struct litestore_ctx
{
    sqlite3* db;
    sqlite3_stmt* save_key;
    sqlite3_stmt* save_raw_data;
};

/* Possible db.objects.type values */
enum
{
    LS_NULL = 0,
    LS_RAW,
    LS_EMPTY_ARRAY,
    LS_EMPTY_OBJECT,
    LS_ARRAY,
    LS_OBJECT
};

/* char* ls_strdup(const char* str) */
/* { */
/*     const size_t LEN = strlen(str) + 1; */
/*     char* copy = (char*)malloc(LEN); */
/*     if (!copy) */
/*     { */
/*         return NULL; */
/*     } */
/*     memcpy(copy, str, LEN - 1); */
/*     copy[LEN] = '\0'; */
/*     return copy; */
/* } */

void ls_print_sqlite_error(litestore_ctx* ctx)
{
    printf("ERROR: %s\n", sqlite3_errmsg(ctx->db));
}

int ls_prepare_statements(litestore_ctx* ctx)
{
    sqlite3_exec(ctx->db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);

    const char* save_key =
        "INSERT INTO objects (name, type) VALUES (?, ?);";
    const char* save_raw_data =
        "INSERT INTO raw_data (id, raw_value) VALUES (?, ?)";
    if (sqlite3_prepare_v2(ctx->db,
                           save_key,
                           -1,
                           &(ctx->save_key),
                           NULL)
        != SQLITE_OK)
    {
        ls_print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    if (sqlite3_prepare_v2(ctx->db,
                           save_raw_data,
                           -1,
                           &(ctx->save_raw_data),
                           NULL)
        != SQLITE_OK)
    {
        ls_print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    return LITESTORE_OK;
}

int ls_finalize_statements(litestore_ctx* ctx)
{
    sqlite3_finalize(ctx->save_key);
    ctx->save_key = NULL;
    sqlite3_finalize(ctx->save_raw_data);
    ctx->save_raw_data = NULL;

    return LITESTORE_OK;
}

int ls_resolve_value_type(const char* value, const size_t value_len)
{
    int type = LS_NULL;

    if (!value || value_len == 0)
    {
        type = LS_NULL;
    }
    else if (value && value_len)
    {
        return LS_RAW;
    }
    /* @todo */

    return type;
}

/*-----------------------------------------*/
/*----------------- SAVE ------------------*/
/*-----------------------------------------*/

int ls_save_key(litestore_ctx* ctx,
             const char* key, const size_t key_len,
             const int data_type)
{
    if (ctx->save_key)
    {
        sqlite3_reset(ctx->save_key);
        if (sqlite3_bind_text(ctx->save_key,
                              1, key, key_len,
                              SQLITE_STATIC) != SQLITE_OK
            || sqlite3_bind_int(ctx->save_key,
                                2, data_type) != SQLITE_OK)
        {
            ls_print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        if (sqlite3_step(ctx->save_key) != SQLITE_DONE)
        {
            ls_print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        return LITESTORE_OK;
    }
    return LITESTORE_ERR;
}

int ls_save_raw_data(litestore_ctx* ctx,
                  const char* key, const size_t key_len,
                  const char* value, const size_t value_len)
{
    if (ctx->save_key && ctx->save_raw_data)
    {
        sqlite3_reset(ctx->save_raw_data);
        if (ls_save_key(ctx, key, key_len, LS_RAW) != LITESTORE_ERR)
        {
            const sqlite3_int64 lastId =
                sqlite3_last_insert_rowid(ctx->db);
            if (sqlite3_bind_int64(ctx->save_raw_data,
                                   1, lastId) != SQLITE_OK
                || sqlite3_bind_blob(ctx->save_raw_data,
                                     2, value, value_len,
                                     SQLITE_STATIC) != SQLITE_OK)
            {
                ls_print_sqlite_error(ctx);
                return LITESTORE_ERR;
            }
            if (sqlite3_step(ctx->save_raw_data) != SQLITE_DONE)
            {
                ls_print_sqlite_error(ctx);
                return LITESTORE_ERR;
            }
            return LITESTORE_OK;
        }
    }
    return LITESTORE_ERR;
}

int ls_save_null(litestore_ctx* ctx,
              const char* key, const size_t key_len)
{
    return ls_save_key(ctx, key, key_len, LS_NULL);
}

/*-----------------------------------------*/
/*------------------ API ------------------*/
/*-----------------------------------------*/

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

        return ls_prepare_statements(*ctx);
    }
    return LITESTORE_ERR;
}

void litestore_close(litestore_ctx* ctx)
{
    if (ctx)
    {
        if (ctx->db)
        {
            ls_finalize_statements(ctx);
            sqlite3_close(ctx->db);
            ctx->db = NULL;
        }
        free(ctx);
    }
}

int litestore_get(litestore_ctx* ctx, const char* key, char** value)
{
    return LITESTORE_ERR;
}

int litestore_save(litestore_ctx* ctx,
                   const char* key, const size_t key_len,
                   const char* value, const size_t value_len)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        switch (ls_resolve_value_type(value, value_len))
        {
            case LS_NULL:
                rv = ls_save_null(ctx, key, key_len);
                break;
            case LS_RAW:
                rv = ls_save_raw_data(ctx, key, key_len, value, value_len);
                break;
        };
    }

    return rv;
}

#ifdef __cplusplus
}  // extern "C"
#endif
