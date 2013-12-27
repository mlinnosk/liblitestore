#include "litestore.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3.h>

#include "litestore_data_parser.h"

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
sqlite3_stmt* begin_tx;
sqlite3_stmt* commit_tx;
sqlite3_stmt* rollback_tx;
sqlite3_stmt* save_obj_data;
int tx_active;
};

/* Possible db.objects.type values */
enum
{
    LS_NULL = 0,
    LS_RAW = 1,
    LS_EMPTY_ARRAY = LS_DP_EMPTY_ARRAY,
    LS_EMPTY_OBJECT = LS_DP_EMPTY_OBJ,
    LS_ARRAY = LS_DP_ARRAY,
    LS_OBJECT = LS_DP_OBJ
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
    const char* begin_tx = "BEGIN IMMEDIATE TRANSACTION;";
    if (sqlite3_prepare_v2(ctx->db,
                           begin_tx,
                           -1,
                           &(ctx->begin_tx),
                           NULL)
        != SQLITE_OK)
    {
        ls_print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    const char* commit_tx = "COMMIT TRANSACTION;";
    if (sqlite3_prepare_v2(ctx->db,
                           commit_tx,
                           -1,
                           &(ctx->commit_tx),
                           NULL)
        != SQLITE_OK)
    {
        ls_print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    const char* rollback_tx = "ROLLBACK TRANSACTION;";
    if (sqlite3_prepare_v2(ctx->db,
                           rollback_tx,
                           -1,
                           &(ctx->rollback_tx),
                           NULL)
        != SQLITE_OK)
    {
        ls_print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    const char* save_obj_data =
        "INSERT OR REPLACE INTO object_data (id, json_key, json_value) "
        "VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(ctx->db,
                           save_obj_data,
                           -1,
                           &(ctx->save_obj_data),
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
    sqlite3_finalize(ctx->begin_tx);
    ctx->begin_tx = NULL;
    sqlite3_finalize(ctx->commit_tx);
    ctx->commit_tx = NULL;
    sqlite3_finalize(ctx->rollback_tx);
    ctx->rollback_tx = NULL;
    sqlite3_finalize(ctx->save_obj_data);
    ctx->save_obj_data = NULL;

    return LITESTORE_OK;
}


/*-----------------------------------------*/
/*----------------- SAVE ------------------*/
/*-----------------------------------------*/

int ls_save_key(litestore* ctx,
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

int ls_save_raw_data(litestore* ctx,
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

/**
 * Struct to pass as the parser user data.
 */
/* typedef struct */
/* { */
/*     litestore_id_t id; */
/*     litestore* ctx; */
/* } id_ctx_t; */

/* /\* called from the parser *\/ */
/* void ls_save_obj_cb(const char* key, const size_t key_len, */
/*                    const char* data, const size_t data_len, */
/*                    void* user_data) */
/* { */
/*     const litestore_id_t id = ((id_ctx_t*)user_data)->id; */
/*     litestore* ctx = ((id_ctx_t*)user_data)->ctx; */

/*     sqlite3_reset(ctx->save_obj_data); */
/*     if (sqlite3_bind_int64(ctx->save_obj_data, 1, id) */
/*         != SQLITE_OK */
/*         || sqlite3_bind_blob(ctx->save_obj_data, */
/*                              2, key, key_len, */
/*                              SQLITE_STATIC) */
/*         != SQLITE_OK */
/*         || sqlite3_bind_blob(ctx->save_obj_data, */
/*                              3, data, data_len, */
/*                              SQLITE_STATIC) */
/*         != SQLITE_OK) */
/*     { */
/*         ls_print_sqlite_error(ctx); */
/*     } */
/*     if (sqlite3_step(ctx->save_obj_data) != SQLITE_DONE) */
/*     { */
/*         ls_print_sqlite_error(ctx); */
/*     } */
/* } */

/* int ls_save_object_data(litestore* ctx, */
/*                        const litestore_id_t new_id, */
/*                        const char* value, const size_t value_len) */
/* { */
/*     if (ctx->save_obj_data) */
/*     { */
/*         id_ctx_t id_ctx = {new_id, ctx}; */
/*         litestore_parser_ctx pctx = {NULL, &ls_save_obj_cb, &id_ctx}; */
/*         return litestore_data_parse(value, value_len, pctx); */
/*     } */
/*     return LITESTORE_ERR; */
/* } */

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
            rv = ls_save_raw_data(ctx, id, value, value_len);
            break;
    }

    return rv;
}

int ls_run_stmt(litestore* ctx, sqlite3_stmt* stmt)
{
    int rv = LITESTORE_ERR;

    if (ctx && stmt)
    {
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            rv = LITESTORE_OK;
        }
        else
        {
            ls_print_sqlite_error(ctx);
        }
    }
    sqlite3_reset(stmt);

    return rv;
}

/**
 * @return 1 on success, 0 on error (boolean value)
 */
int ls_opt_begin_tx(litestore* ctx)
{
    if (!ctx->tx_active && litestore_begin_tx(ctx) == LITESTORE_OK)
    {
        return 1;
    }
    return 0;
}

/**
 * @return Return value of the tx function called.
 */
int ls_opt_end_tx(litestore* ctx, int status)
{
    if (status == LITESTORE_OK)
    {
        return litestore_commit_tx(ctx);
    }
    return litestore_rollback_tx(ctx);
}

/*-----------------------------------------*/
/*------------------ API ------------------*/
/*-----------------------------------------*/

void* litestore_native_ctx(litestore* ctx)
{
    return ctx->db;
}

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

int litestore_begin_tx(litestore* ctx)
{
    const int rv = ls_run_stmt(ctx, ctx->begin_tx);
    if (rv == LITESTORE_OK)
    {
        ctx->tx_active = 1;
    }
    return rv;
}

int litestore_commit_tx(litestore* ctx)
{
    const int rv = ls_run_stmt(ctx, ctx->commit_tx);
    if (rv == LITESTORE_OK)
    {
        ctx->tx_active = 0;
    }
    return rv;
}

int litestore_rollback_tx(litestore* ctx)
{
    const int rv = ls_run_stmt(ctx, ctx->rollback_tx);
    if (rv == LITESTORE_OK)
    {
        ctx->tx_active = 0;
    }
    return rv;
}

int litestore_get_null(litestore* ctx,
                       const char* key, const size_t key_len)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        int own_tx = ls_opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int type = -1;
        rv = ls_get_key_type(ctx, key, key_len, &id, &type);

        if (rv == LITESTORE_OK && type != LS_NULL)
        {
            rv = LITESTORE_ERR;
        }

        if (own_tx)
        {
            ls_opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

int litestore_save_null(litestore* ctx,
                        const char* key, const size_t key_len)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        int own_tx = ls_opt_begin_tx(ctx);

        litestore_id_t new_id = 0;
        rv = ls_save_key(ctx, key, key_len, LS_NULL, &new_id);

        if (own_tx)
        {
            ls_opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

int litestore_update_null(litestore* ctx,
                          const char* key, const size_t key_len)
{
    int rv = LITESTORE_ERR;
    if (ctx && key && key_len > 0)
    {
        int own_tx = ls_opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int old_type = -1;
        rv = ls_get_key_type(ctx, key, key_len, &id, &old_type);
        if (rv == LITESTORE_OK)
        {
            rv = ls_update_null(ctx, id, old_type);
            if (rv == LITESTORE_OK && old_type != LS_NULL)
            {
                rv = ls_update_type(ctx, id, LS_NULL);
            }
        }
        else if (rv == LITESTORE_UNKNOWN_ENTITY)
        {
            rv = litestore_save_null(ctx, key, key_len);
        }

        if (own_tx)
        {
            ls_opt_end_tx(ctx, rv);
        }
    }
    return rv;
}

int litestore_get_raw(litestore* ctx,
                      const char* key, const size_t key_len,
                      char** value, size_t* value_len)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        int own_tx = ls_opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int type = -1;
        rv = ls_get_key_type(ctx, key, key_len, &id, &type);

        if (rv == LITESTORE_OK && type == LS_RAW)
        {
            rv = ls_get_raw_data(ctx, id, value, value_len);
        }
        else
        {
            rv = LITESTORE_ERR;
        }

        if (own_tx)
        {
            ls_opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

int litestore_save_raw(litestore* ctx,
                       const char* key, const size_t key_len,
                       const char* value, const size_t value_len)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0 && value && value_len > 0)
    {
        int own_tx = ls_opt_begin_tx(ctx);

        litestore_id_t new_id = 0;
        rv = ls_save_key(ctx, key, key_len, LS_RAW, &new_id);
        if (rv == LITESTORE_OK)
        {
            rv = ls_save_raw_data(ctx, new_id, value, value_len);
        }

        if (own_tx)
        {
            ls_opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

int litestore_update_raw(litestore* ctx,
                         const char* key, const size_t key_len,
                         const char* value, const size_t value_len)
{
    int rv = LITESTORE_ERR;
    if (ctx && key && key_len > 0)
    {
        int own_tx = ls_opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int old_type = -1;
        rv = ls_get_key_type(ctx, key, key_len, &id, &old_type);
        if (rv == LITESTORE_OK)
        {
            rv = ls_update_raw_data(ctx,
                                    id, old_type,
                                    value, value_len);
            if (rv == LITESTORE_OK && old_type != LS_RAW)
            {
                rv = ls_update_type(ctx, id, LS_RAW);
            }
        }
        else if (rv == LITESTORE_UNKNOWN_ENTITY)
        {
            rv = litestore_save_raw(ctx, key, key_len, value, value_len);
        }

        if (own_tx)
        {
            ls_opt_end_tx(ctx, rv);
        }
    }
    return rv;
}

int litestore_delete(litestore* ctx,
                     const char* key, const size_t key_len)
{
    int rv = LITESTORE_ERR;

    /* delete should cascade */
    if (ctx && key && key_len > 0 && ctx->delete_key)
    {
        int own_tx = ls_opt_begin_tx(ctx);

        sqlite3_reset(ctx->delete_key);
        if (sqlite3_bind_text(ctx->delete_key,
                              1, key, key_len,
                              SQLITE_STATIC) == SQLITE_OK)
        {
            if (sqlite3_step(ctx->delete_key) == SQLITE_DONE)
            {
                rv = (sqlite3_changes(ctx->db) == 1 ?
                      LITESTORE_OK : LITESTORE_UNKNOWN_ENTITY);
            }
            else
            {
                ls_print_sqlite_error(ctx);
                rv = LITESTORE_ERR;
            }
        }
        else
        {
            ls_print_sqlite_error(ctx);
            rv = LITESTORE_ERR;
        }

        if (own_tx)
        {
            ls_opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

#ifdef __cplusplus
}  // extern "C"
#endif
