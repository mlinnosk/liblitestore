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
struct litestore
{
    sqlite3* db;
    sqlite3_stmt* save_key;
    sqlite3_stmt* save_raw_data;
    sqlite3_stmt* delete_key;
    sqlite3_stmt* get_key;
    sqlite3_stmt* get_raw_data;
    sqlite3_stmt* update_type;
    sqlite3_stmt* delete_raw_data;
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

/* The native db ID type */
typedef sqlite3_int64 litestore_id_t;

/*
 * Allocates a buffer and copies the str data of len to the new buffer.
 * The copy will be null terminated
 */
char* ls_strdup(const void* str, const size_t len)
{
    const size_t LEN = len + 1; /* extra for terminator */
    char* copy = (char*)malloc(LEN);
    if (!copy)
    {
        return NULL;
    }
    memcpy(copy, str, len);
    copy[len] = 0;
    return copy;
}

void ls_print_sqlite_error(litestore* ctx)
{
    printf("ERROR: %s\n", sqlite3_errmsg(ctx->db));
}

int ls_prepare_statements(litestore* ctx)
{
    sqlite3_exec(ctx->db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);

    const char* save_key =
        "INSERT INTO objects (name, type) VALUES (?, ?);";
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
    const char* save_raw_data =
        "INSERT OR REPLACE INTO raw_data (id, raw_value) VALUES (?, ?);";
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
    const char* delete_key =
        "DELETE FROM objects WHERE name = ?;";
    if (sqlite3_prepare_v2(ctx->db,
                           delete_key,
                           -1,
                           &(ctx->delete_key),
                           NULL)
        != SQLITE_OK)
    {
        ls_print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    const char* get_key =
        "SELECT id, type FROM objects WHERE name = ?;";
    if (sqlite3_prepare_v2(ctx->db,
                           get_key,
                           -1,
                           &(ctx->get_key),
                           NULL)
        != SQLITE_OK)
    {
        ls_print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    const char* get_raw_data =
        "SELECT raw_value FROM raw_data WHERE id = ?;";
    if (sqlite3_prepare_v2(ctx->db,
                           get_raw_data,
                           -1,
                           &(ctx->get_raw_data),
                           NULL)
        != SQLITE_OK)
    {
        ls_print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    const char* update_type =
        "UPDATE objects SET type = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(ctx->db,
                           update_type,
                           -1,
                           &(ctx->update_type),
                           NULL)
        != SQLITE_OK)
    {
        ls_print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    const char* delete_raw_data =
        "DELETE FROM raw_data WHERE id = ?;";
    if (sqlite3_prepare_v2(ctx->db,
                           delete_raw_data,
                           -1,
                           &(ctx->delete_raw_data),
                           NULL)
        != SQLITE_OK)
    {
        ls_print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    return LITESTORE_OK;
}

int ls_finalize_statements(litestore* ctx)
{
    sqlite3_finalize(ctx->save_key);
    ctx->save_key = NULL;
    sqlite3_finalize(ctx->save_raw_data);
    ctx->save_raw_data = NULL;
    sqlite3_finalize(ctx->delete_key);
    ctx->delete_key = NULL;
    sqlite3_finalize(ctx->get_key);
    ctx->get_key = NULL;
    sqlite3_finalize(ctx->get_raw_data);
    ctx->get_raw_data = NULL;
    sqlite3_finalize(ctx->update_type);
    ctx->update_type = NULL;
    sqlite3_finalize(ctx->delete_raw_data);
    ctx->delete_raw_data = NULL;

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

int ls_put_key(litestore* ctx,
               const char* key, const size_t key_len,
               const int data_type,
               litestore_id_t* id)
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
        if (id)
        {
            *id = sqlite3_last_insert_rowid(ctx->db);
        }
        return LITESTORE_OK;
    }
    return LITESTORE_ERR;
}

int ls_put_raw_data(litestore* ctx,
                    const litestore_id_t new_id,
                    const char* value, const size_t value_len)
{
    if (ctx->save_raw_data)
    {
        sqlite3_reset(ctx->save_raw_data);
        if (sqlite3_bind_int64(ctx->save_raw_data, 1, new_id)
            != SQLITE_OK
            || sqlite3_bind_blob(ctx->save_raw_data,
                                 2, value, value_len,
                                 SQLITE_STATIC)
            != SQLITE_OK)
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
    return LITESTORE_ERR;
}

/*-----------------------------------------*/
/*----------------- GET -------------------*/
/*-----------------------------------------*/

int ls_get_key_type(litestore* ctx,
                    const char* key, const size_t key_len,
                    litestore_id_t* id, int* type)
{
    if (ctx->get_key && key && key_len > 0)
    {
        sqlite3_reset(ctx->get_key);
        if (sqlite3_bind_text(ctx->get_key,
                              1, key, key_len,
                              SQLITE_STATIC) != SQLITE_OK)
        {
            ls_print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        /* expect only one aswer */
        if (sqlite3_step(ctx->get_key) != SQLITE_ROW)
        {
            *id = 0;
            *type = -1;
            return LITESTORE_UNKNOWN_ENTITY;
        }
        *id = sqlite3_column_int64(ctx->get_key, 0);
        *type = sqlite3_column_int(ctx->get_key, 1);
        return LITESTORE_OK;
    }
    return LITESTORE_ERR;
}

int ls_get_raw_data(litestore* ctx,
                    const litestore_id_t id,
                    char** value, size_t* value_len)
{
    if (ctx->get_raw_data)
    {
        sqlite3_reset(ctx->get_raw_data);
        if (sqlite3_bind_int64(ctx->get_raw_data,
                               1, id) != SQLITE_OK)
        {
            ls_print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        /* expect only one aswer */
        if (sqlite3_step(ctx->get_raw_data) != SQLITE_ROW)
        {
            ls_print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        const void* raw_data = sqlite3_column_blob(ctx->get_raw_data,
                                                   0);
        const int bytes = sqlite3_column_bytes(ctx->get_raw_data,
                                               0);
        if (bytes > 0)
        {
            *value = ls_strdup(raw_data, bytes);
            if (*value)
            {
                *value_len = bytes;
                return LITESTORE_OK;
            }
        }
    }
    return LITESTORE_ERR;
}

int ls_update_type(litestore* ctx,
                   const litestore_id_t id, const int type)
{
    sqlite3_reset(ctx->update_type);
    if (sqlite3_bind_int(ctx->update_type, 1, type) != SQLITE_OK
        || sqlite3_bind_int64(ctx->update_type, 2, id) != SQLITE_OK)
    {
        ls_print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    if (sqlite3_step(ctx->update_type) != SQLITE_DONE)
    {
        ls_print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }

    return LITESTORE_OK;
}

int ls_delete_raw_data(litestore*ctx, const litestore_id_t id)
{
    if (ctx->delete_raw_data)
    {
        sqlite3_reset(ctx->delete_raw_data);
        if ((sqlite3_bind_int64(ctx->delete_raw_data, 1, id) != SQLITE_OK))
        {
            ls_print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        if (sqlite3_step(ctx->delete_raw_data) != SQLITE_DONE)
        {
            ls_print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
    }
    else
    {
        return LITESTORE_ERR;
    }
    return LITESTORE_OK;
}

int ls_update_null(litestore* ctx,
                   const litestore_id_t id, const int old_type)
{
    int rv = LITESTORE_OK;

    switch (old_type)
    {
        case LS_RAW:
            rv = ls_delete_raw_data(ctx, id);
            break;
    }

    return rv;
}

int ls_update_raw_data(litestore* ctx,
                       const litestore_id_t id, const int old_type,
                       const char* value, const size_t value_len)
{
    int rv = LITESTORE_OK;

    switch (old_type)
    {
        case LS_NULL:
        case LS_RAW:
            rv = ls_put_raw_data(ctx, id, value, value_len);
            break;
    }

    return rv;
}


/*-----------------------------------------*/
/*------------------ API ------------------*/
/*-----------------------------------------*/

int litestore_open(const char* db_file_name, litestore** ctx)
{
    *ctx = (litestore*)malloc(sizeof(litestore));
    if (*ctx)
    {
        memset(*ctx, 0, sizeof(litestore));

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

void litestore_close(litestore* ctx)
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

void* litestore_native_ctx(litestore* ctx)
{
    return ctx->db;
}

int litestore_get(litestore* ctx,
                  const char* key, const size_t key_len,
                  char** value, size_t* value_len)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        litestore_id_t id = 0;
        int type = -1;
        rv = ls_get_key_type(ctx, key, key_len, &id, &type);
        if (rv == LITESTORE_OK)
        {
            switch (type)
            {
                case LS_NULL:
                {
                    if (value)
                    {
                        *value = NULL;
                    }
                    if (value_len)
                    {
                        *value_len = 0;
                    }
                    rv = LITESTORE_OK;
                }
                break;
                case LS_RAW:
                {
                    rv = ls_get_raw_data(ctx, id, value, value_len);
                }
                break;
            }
        }
    }

    return rv;
}

int litestore_put(litestore* ctx,
                  const char* key, const size_t key_len,
                  const char* value, const size_t value_len)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        const int type = ls_resolve_value_type(value, value_len);
        litestore_id_t new_id = 0;
        rv = ls_put_key(ctx, key, key_len, type, &new_id);
        if (rv == LITESTORE_OK)
        {
            switch (type)
            {
                case LS_NULL:
                {
                    rv = LITESTORE_OK;
                }
                break;

                case LS_RAW:
                {
                    rv = ls_put_raw_data(ctx, new_id, value, value_len);
                }
                break;
            }
        }
    }

    return rv;
}

int litestore_put_raw(litestore* ctx,
                      const char* key, const size_t key_len,
                      const char* value, const size_t value_len)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0 && value && value_len > 0)
    {
        litestore_id_t new_id = 0;
        rv = ls_put_key(ctx, key, key_len, LS_RAW, &new_id);
        if (rv == LITESTORE_OK)
        {
            rv = ls_put_raw_data(ctx, new_id, value, value_len);
        }
    }

    return rv;
}

int litestore_update(litestore* ctx,
                     const char* key, const size_t key_len,
                     const char* value, const size_t value_len)
{
    int rv = LITESTORE_ERR;
    if (ctx && key && key_len > 0)
    {
        litestore_id_t id = 0;
        int old_type = -1;
        rv = ls_get_key_type(ctx, key, key_len, &id, &old_type);
        if (rv == LITESTORE_OK)
        {
            const int new_type = ls_resolve_value_type(value, value_len);
            switch (new_type)
            {
                case LS_NULL:
                    rv = ls_update_null(ctx, id, old_type);
                    break;
                case LS_RAW:
                    rv = ls_update_raw_data(ctx,
                                            id, old_type,
                                            value, value_len);
                    break;
            }
            if (rv == LITESTORE_OK && old_type != new_type)
            {
                rv = ls_update_type(ctx, id, new_type);
            }
        }
        else if (rv == LITESTORE_UNKNOWN_ENTITY)
        {
            rv = litestore_put(ctx, key, key_len, value, value_len);
        }
    }
    return rv;
}

int litestore_delete(litestore* ctx,
                     const char* key, const size_t key_len)
{
    /* delete should cascade */
    if (ctx && key && key_len > 0 && ctx->delete_key)
    {
        sqlite3_reset(ctx->delete_key);
        if (sqlite3_bind_text(ctx->delete_key,
                              1, key, key_len,
                              SQLITE_STATIC) != SQLITE_OK)
        {
            ls_print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        if (sqlite3_step(ctx->delete_key) != SQLITE_DONE)
        {
            ls_print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        return (sqlite3_changes(ctx->db) == 1 ?
                LITESTORE_OK : LITESTORE_UNKNOWN_ENTITY);
    }
    return LITESTORE_ERR;
}

#ifdef __cplusplus
}  // extern "C"
#endif
