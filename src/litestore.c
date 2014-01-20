/**
 * Copyright (c) 2014 Markku Linnoskivi
 *
 * See the file LICENSE.txt for copying permission.
 */
#include "litestore.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3.h>


#ifdef __cplusplus
extern "C"
#endif

#define UNUSED(x) (void)(x)

/**
 * The DB schema.
 */
#define LITESTORE_SCHEMA_V1                             \
    "CREATE TABLE IF NOT EXISTS meta("                  \
    "       schema_version INTEGER NOT NULL DEFAULT 1"  \
    ");"                                                \
    "CREATE TABLE IF NOT EXISTS objects("               \
    "       id INTEGER PRIMARY KEY NOT NULL,"           \
    "       name TEXT NOT NULL UNIQUE,"                 \
    "       type INTEGER NOT NULL"                      \
    ");"                                                \
    "CREATE TABLE IF NOT EXISTS raw_data("              \
    "       id INTEGER NOT NULL,"                       \
    "       raw_value BLOB NOT NULL,"                   \
    "       FOREIGN KEY(id) REFERENCES objects(id)"     \
    "       ON DELETE CASCADE ON UPDATE RESTRICT"       \
    ");"                                                \
    "CREATE TABLE IF NOT EXISTS kv_data("               \
    "       id INTEGER NOT NULL,"                       \
    "       kv_key BLOB NOT NULL,"                      \
    "       kv_value BLOB NOT NULL,"                    \
    "       FOREIGN KEY(id) REFERENCES objects(id)"     \
    "       ON DELETE CASCADE ON UPDATE RESTRICT"       \
    ");"                                                \
    "CREATE TABLE IF NOT EXISTS array_data("            \
    "       id INTEGER NOT NULL,"                       \
    "       array_index INTEGER NOT NULL,"              \
    "       array_value BLOB NOT NULL,"                 \
    "       FOREIGN KEY(id) REFERENCES objects(id)"     \
    "       ON DELETE CASCADE ON UPDATE RESTRICT"       \
    ");"                                                \

/**
 * The LiteStore object.
 */
struct litestore
{
sqlite3* db;
/* tx */
sqlite3_stmt* begin_tx;
sqlite3_stmt* commit_tx;
sqlite3_stmt* rollback_tx;
int tx_active;
/* object */
sqlite3_stmt* save_key;
sqlite3_stmt* get_key;
sqlite3_stmt* delete_key;
sqlite3_stmt* update_type;
/* raw */
sqlite3_stmt* save_raw_data;
sqlite3_stmt* get_raw_data;
sqlite3_stmt* update_raw_data;
sqlite3_stmt* delete_raw_data;
/* array */
sqlite3_stmt* save_array_data;
sqlite3_stmt* get_array_data;
sqlite3_stmt* update_array_data;
sqlite3_stmt* delete_array_data;
/* kv */
sqlite3_stmt* save_kv_data;
sqlite3_stmt* get_kv_data;
sqlite3_stmt* update_kv_data;
sqlite3_stmt* delete_kv_data;
};

/* Possible db.objects.type values */
enum
{
    LS_NULL = 0,
    LS_RAW = 1,
    LS_ARRAY = 2,
    LS_KV = 3
};

/* The native db ID type */
typedef sqlite3_int64 litestore_id_t;


/*-----------------------------------------*/
/*----------------- HELPERS ---------------*/
/*-----------------------------------------*/
static
void print_sqlite_error(litestore* ctx)
{
    printf("ERROR: %s\n", sqlite3_errmsg(ctx->db));
}

static
int run_stmt(litestore* ctx, sqlite3_stmt* stmt)
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
            print_sqlite_error(ctx);
        }
    }
    sqlite3_reset(stmt);

    return rv;
}

/**
 * @return 1 on success, 0 on error (boolean value)
 */
static
int opt_begin_tx(litestore* ctx)
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
static
int opt_end_tx(litestore* ctx, int status)
{
    if (status == LITESTORE_OK)
    {
        return litestore_commit_tx(ctx);
    }
    return litestore_rollback_tx(ctx);
}


/*-----------------------------------------*/
/*----------------- INIT ------------------*/
/*-----------------------------------------*/
static
int init_db(litestore* ctx)
{
    if (sqlite3_exec(ctx->db, LITESTORE_SCHEMA_V1, NULL, NULL, NULL)
        == SQLITE_OK)
    {
        /* @todo Check schema version */
        /* @note For some reason the pragma won't work if run
           inside the same TX as schema. */
        return (sqlite3_exec(ctx->db, "PRAGMA foreign_keys = ON;",
                             NULL, NULL, NULL)
                == SQLITE_OK) ? LITESTORE_OK : LITESTORE_ERR;
    }
    return LITESTORE_ERR;
}

static
int prepare_stmt(litestore* ctx, const char* sql, sqlite3_stmt** stmt)
{
    if (sqlite3_prepare_v2(ctx->db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        return LITESTORE_ERR;
    }
    return LITESTORE_OK;
}

static
int prepare_statements(litestore* ctx)
{
    /* object */
    if (prepare_stmt(ctx,
                     "INSERT INTO objects (name, type) VALUES (?, ?);",
                     &(ctx->save_key)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "SELECT id, type FROM objects WHERE name = ?;",
                        &(ctx->get_key)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "DELETE FROM objects WHERE name = ?;",
                        &(ctx->delete_key)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "UPDATE objects SET type = ? WHERE id = ?;",
                        &(ctx->update_type)) != LITESTORE_OK
        /* tx */
        || prepare_stmt(ctx,
                        "BEGIN IMMEDIATE TRANSACTION;",
                        &(ctx->begin_tx)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "COMMIT TRANSACTION;",
                        &(ctx->commit_tx)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "ROLLBACK TRANSACTION;",
                        &(ctx->rollback_tx)) != LITESTORE_OK
        /* raw */
        || prepare_stmt(ctx,
                        "INSERT INTO raw_data (id, raw_value) VALUES (?, ?);",
                        &(ctx->save_raw_data)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "SELECT raw_value FROM raw_data WHERE id = ?;",
                        &(ctx->get_raw_data)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "UPDATE raw_data SET raw_value = ? WHERE id = ?;",
                        &(ctx->update_raw_data)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "DELETE FROM raw_data WHERE id = ?;",
                        &(ctx->delete_raw_data)) != LITESTORE_OK
        /* array */
        || prepare_stmt(ctx,
                        "INSERT INTO array_data (id, array_index, array_value) "
                        "VALUES (?, ?, ?);",
                        &(ctx->save_array_data)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "SELECT array_index, array_value FROM array_data "
                        "WHERE id = ? ORDER BY array_index;",
                        &(ctx->get_array_data)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "UPDATE array_data SET array_value = ? "
                        "WHERE id = ? AND array_index = ?;",
                        &(ctx->update_array_data)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "DELETE FROM array_data WHERE id = ?;",
                        &(ctx->delete_array_data)) != LITESTORE_OK
        /* kv */
        || prepare_stmt(ctx,
                        "INSERT INTO kv_data (id, kv_key, kv_value) "
                        "VALUES (?, ?, ?);",
                        &(ctx->save_kv_data)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "SELECT kv_key, kv_value FROM kv_data WHERE id = ?;",
                        &(ctx->get_kv_data)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "UPDATE kv_data SET kv_value = ? "
                        "WHERE id = ? AND kv_key = ?;",
                        &(ctx->update_kv_data)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "DELETE FROM kv_data WHERE id = ?;",
                        &(ctx->delete_kv_data)) != LITESTORE_OK)
    {
        print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }

    return LITESTORE_OK;
}

static
void finalize_stmt(sqlite3_stmt** stmt)
{
    sqlite3_finalize(*stmt);
    *stmt = NULL;
}

static
int finalize_statements(litestore* ctx)
{
    /* object */
    finalize_stmt(&(ctx->save_key));
    finalize_stmt(&(ctx->get_key));
    finalize_stmt(&(ctx->delete_key));
    finalize_stmt(&(ctx->update_type));
    /* tx */
    finalize_stmt(&(ctx->begin_tx));
    finalize_stmt(&(ctx->commit_tx));
    finalize_stmt(&(ctx->rollback_tx));
    /* raw */
    finalize_stmt(&(ctx->save_raw_data));
    finalize_stmt(&(ctx->get_raw_data));
    finalize_stmt(&(ctx->update_raw_data));
    finalize_stmt(&(ctx->delete_raw_data));
    /* array */
    finalize_stmt(&(ctx->save_array_data));
    finalize_stmt(&(ctx->get_array_data));
    finalize_stmt(&(ctx->update_array_data));
    finalize_stmt(&(ctx->delete_array_data));
    /* kv */
    finalize_stmt(&(ctx->save_kv_data));
    finalize_stmt(&(ctx->get_kv_data));
    finalize_stmt(&(ctx->update_kv_data));
    finalize_stmt(&(ctx->delete_kv_data));

    return LITESTORE_OK;
}


/*-----------------------------------------*/
/*----------------- SAVE ------------------*/
/*-----------------------------------------*/
static
int save_key(litestore* ctx,
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
            print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        if (sqlite3_step(ctx->save_key) != SQLITE_DONE)
        {
            print_sqlite_error(ctx);
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

static
int save_raw_data(litestore* ctx, litestore_id_t new_id, void* value)
{
    int rv = LITESTORE_ERR;

    litestore_blob_t* blob = (litestore_blob_t*)value;
    if (ctx->save_raw_data && blob->data && blob->size > 0)
    {
        if (sqlite3_bind_int64(ctx->save_raw_data, 1, new_id) == SQLITE_OK
            && sqlite3_bind_blob(ctx->save_raw_data,
                                 2, blob->data, blob->size,
                                 SQLITE_STATIC) == SQLITE_OK
            && sqlite3_step(ctx->save_raw_data) == SQLITE_DONE)
        {
            rv = LITESTORE_OK;
        }
        else
        {
            print_sqlite_error(ctx);
        }
        sqlite3_reset(ctx->save_raw_data);
    }
    return rv;
}

static
int save_array(litestore* ctx, const litestore_id_t id,
               const unsigned index,
               const void* value, const size_t value_len)
{
    int rv = LITESTORE_ERR;

    if (value && value_len > 0)
    {
        if (sqlite3_bind_int64(ctx->save_array_data, 1, id) == SQLITE_OK
            && sqlite3_bind_int64(ctx->save_array_data, 2, index) == SQLITE_OK
            && sqlite3_bind_blob(ctx->save_array_data, 3, value, value_len,
                                 SQLITE_STATIC) == SQLITE_OK
            && sqlite3_step(ctx->save_array_data) == SQLITE_DONE)
        {
            rv = LITESTORE_OK;
        }
        else
        {
            print_sqlite_error(ctx);
        }
        sqlite3_reset(ctx->save_array_data);
    }

    return rv;
}

static
int save_array_data(litestore* ctx, const litestore_id_t new_id, void* data)
{
    litestore_array_iterator* iter = (litestore_array_iterator*)data;
    unsigned index = 0;
    const void* v = NULL;
    size_t v_len = 0;

    for (iter->begin(iter->user_data);
         !iter->end(iter->user_data);
         iter->next(iter->user_data))
    {
        iter->value(iter->user_data, &v, &v_len);
        if (save_array(ctx, new_id, index, v, v_len) != LITESTORE_OK)
        {
            return LITESTORE_ERR;
        }
        v = NULL;
        v_len = 0;
        ++index;
    }

    return LITESTORE_OK;
}

static
int save_kv(litestore* ctx, const litestore_id_t id,
            const void* key, const size_t key_len,
            const void* value, const size_t value_len)
{
    int rv = LITESTORE_ERR;

    if (key && key_len > 0 && value && value_len > 0)
    {
        if (sqlite3_bind_int64(ctx->save_kv_data, 1, id) == SQLITE_OK
            && sqlite3_bind_blob(ctx->save_kv_data, 2, key, key_len,
                                 SQLITE_STATIC) == SQLITE_OK
            && sqlite3_bind_blob(ctx->save_kv_data, 3, value, value_len,
                                 SQLITE_STATIC) == SQLITE_OK
            && sqlite3_step(ctx->save_kv_data) == SQLITE_DONE)
        {
            rv = LITESTORE_OK;
        }
        else
        {
            print_sqlite_error(ctx);
        }
        sqlite3_reset(ctx->save_kv_data);
    }

    return rv;
}

static
int save_kv_data(litestore* ctx, const litestore_id_t new_id, void* data)
{
    litestore_kv_iterator* iter = (litestore_kv_iterator*)data;
    const void* k = NULL;
    size_t k_len = 0;
    const void* v = NULL;
    size_t v_len = 0;

    for (iter->begin(iter->user_data);
         !iter->end(iter->user_data);
         iter->next(iter->user_data))
    {
        iter->value(iter->user_data, &k, &k_len, &v, &v_len);
        if (save_kv(ctx, new_id, k, k_len, v, v_len) != LITESTORE_OK)
        {
            return LITESTORE_ERR;
        }
        k = NULL;
        k_len = 0;
        v = NULL;
        v_len = 0;
    }

    return LITESTORE_OK;
}

typedef struct
{
    int object_type;
    int (*save)(litestore*, litestore_id_t, void*);
    void* data;
} save_ctx;

static
int gen_save(litestore* ctx,
             const char* key, const size_t key_len,
             save_ctx op)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0 && op.save && op.data)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t new_id = 0;
        rv = save_key(ctx, key, key_len, op.object_type, &new_id);
        if (rv == LITESTORE_OK)
        {
            rv = (*op.save)(ctx, new_id, op.data);
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

/*-----------------------------------------*/
/*----------------- GET -------------------*/
/*-----------------------------------------*/
static
int get_object_type(litestore* ctx,
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
            print_sqlite_error(ctx);
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

static
int get_raw_data(litestore* ctx, const litestore_id_t id,
                 const void* key, const size_t key_len,
                 void* cb, void* user_data)
{
    UNUSED(key);
    UNUSED(key_len);
    int rv = LITESTORE_ERR;

    if (ctx->get_raw_data && cb)
    {
        litestore_get_raw_cb callback = (litestore_get_raw_cb)cb;

        if (sqlite3_bind_int64(ctx->get_raw_data, 1, id) == SQLITE_OK
            && sqlite3_step(ctx->get_raw_data) == SQLITE_ROW)
        {
            const void* raw_data =
                sqlite3_column_blob(ctx->get_raw_data, 0);
            const int bytes =
                sqlite3_column_bytes(ctx->get_raw_data, 0);
            if (bytes > 0)
            {
                rv = (*callback)(
                    litestore_make_blob(raw_data, bytes), user_data);
            }
        }
        else
        {
            print_sqlite_error(ctx);
        }
        sqlite3_reset(ctx->get_raw_data);
    }

    return rv;
}

static
int get_array_data(litestore* ctx,
                   const litestore_id_t id,
                   const void* key, const size_t key_len,
                   void* cb, void* user_data)
{
    int rv = LITESTORE_ERR;

    if (ctx->get_array_data && cb)
    {
        litestore_get_array_cb callback = (litestore_get_array_cb)cb;

        if (sqlite3_bind_int64(ctx->get_array_data, 1, id) != SQLITE_OK)
        {
            print_sqlite_error(ctx);
        }
        else
        {
            int rc = 0;
            while ((rc = sqlite3_step(ctx->get_array_data)) != SQLITE_DONE)
            {
                if (rc == SQLITE_ROW)
                {
                    const unsigned index =
                        (unsigned)sqlite3_column_int64(ctx->get_array_data, 0);
                    const void* v =
                        sqlite3_column_blob(ctx->get_array_data, 1);
                    const int v_len =
                        sqlite3_column_bytes(ctx->get_array_data, 1);
                    if ((*callback)(litestore_slice(key, 0, key_len),
                                    index, litestore_make_blob(v, v_len),
                                    user_data)
                        != LITESTORE_OK)
                    {
                        break;
                    }
                }
                else
                {
                    print_sqlite_error(ctx);
                    break;
                }
            }  /* while */
            if (rc == SQLITE_DONE)
            {
                rv = LITESTORE_OK;
            }
        }
        sqlite3_reset(ctx->get_array_data);
    }

    return rv;
}

static
int get_kv_data(litestore* ctx,
                const litestore_id_t id,
                const void* key, const size_t key_len,
                void* cb, void* user_data)
{
    int rv = LITESTORE_ERR;

    if (ctx->get_kv_data && cb)
    {
        litestore_get_kv_cb callback = (litestore_get_kv_cb)cb;

        if (sqlite3_bind_int64(ctx->get_kv_data, 1, id) != SQLITE_OK)
        {
            print_sqlite_error(ctx);
        }
        else
        {
            int rc = 0;
            while ((rc = sqlite3_step(ctx->get_kv_data)) != SQLITE_DONE)
            {
                if (rc == SQLITE_ROW)
                {
                    const void* k =
                        sqlite3_column_blob(ctx->get_kv_data, 0);
                    const int k_len =
                        sqlite3_column_bytes(ctx->get_kv_data, 0);
                    const void* v =
                        sqlite3_column_blob(ctx->get_kv_data, 1);
                    const int v_len =
                        sqlite3_column_bytes(ctx->get_kv_data, 1);
                    if ((*callback)(litestore_slice(key, 0, key_len),
                                    litestore_make_blob(k, k_len),
                                    litestore_make_blob(v, v_len),
                                    user_data)
                        != LITESTORE_OK)
                    {
                        break;
                    }
                }
                else
                {
                    print_sqlite_error(ctx);
                    break;
                }
            }  /* while */
            if (rc == SQLITE_DONE)
            {
                rv = LITESTORE_OK;
            }
        }
        sqlite3_reset(ctx->get_kv_data);
    }

    return rv;
}

typedef struct
{
    int object_type;
    int (*get)(litestore*, const litestore_id_t,
               const void*, const size_t,
               void*, void*);
    void* callback;
    void* user_data;
} get_ctx;

static
int gen_get(litestore* ctx,
            const char* key, const size_t key_len,
            get_ctx op)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0 && op.get)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int type = -1;
        rv = get_object_type(ctx, key, key_len, &id, &type);

        if (rv == LITESTORE_OK && type == op.object_type)
        {
            rv = (*op.get)(ctx, id, key, key_len, op.callback, op.user_data);
        }
        else
        {
            rv = LITESTORE_ERR;
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}


/*-----------------------------------------*/
/*----------------- DELETE ----------------*/
/*-----------------------------------------*/
static
int delete_raw_data(litestore*ctx, const litestore_id_t id)
{
    if (ctx->delete_raw_data)
    {
        sqlite3_reset(ctx->delete_raw_data);
        if ((sqlite3_bind_int64(ctx->delete_raw_data, 1, id) != SQLITE_OK))
        {
            print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        if (sqlite3_step(ctx->delete_raw_data) != SQLITE_DONE)
        {
            print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
    }
    else
    {
        return LITESTORE_ERR;
    }
    return LITESTORE_OK;
}

static
int delete_kv_data(litestore* ctx, const litestore_id_t id)
{
    int rv = LITESTORE_ERR;

    if (ctx->delete_kv_data)
    {
        if (sqlite3_bind_int64(ctx->delete_kv_data, 1, id) == SQLITE_OK
            && sqlite3_step(ctx->delete_kv_data) == SQLITE_DONE)
        {
            rv = LITESTORE_OK;
        }
        else
        {
            print_sqlite_error(ctx);
        }
        sqlite3_reset(ctx->delete_kv_data);
    }

    return rv;
}

static
int delete_array_data(litestore* ctx, const litestore_id_t id)
{
    int rv = LITESTORE_ERR;

    if (ctx->delete_array_data)
    {
        if (sqlite3_bind_int64(ctx->delete_array_data, 1, id) == SQLITE_OK
            && sqlite3_step(ctx->delete_array_data) == SQLITE_DONE)
        {
            rv = LITESTORE_OK;
        }
        else
        {
            print_sqlite_error(ctx);
        }
        sqlite3_reset(ctx->delete_array_data);
    }

    return rv;
}


/*-----------------------------------------*/
/*----------------- UPDATE ----------------*/
/*-----------------------------------------*/
static
int update_object_type(litestore* ctx,
                       const litestore_id_t id, const int type)
{
    sqlite3_reset(ctx->update_type);
    if (sqlite3_bind_int(ctx->update_type, 1, type) != SQLITE_OK
        || sqlite3_bind_int64(ctx->update_type, 2, id) != SQLITE_OK)
    {
        print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    if (sqlite3_step(ctx->update_type) != SQLITE_DONE)
    {
        print_sqlite_error(ctx);
        return LITESTORE_ERR;
    }

    return LITESTORE_OK;
}

static
int update_null(litestore* ctx,
                const litestore_id_t id, const int old_type)
{
    int rv = LITESTORE_OK;

    switch (old_type)
    {
        case LS_RAW:
            rv = delete_raw_data(ctx, id);
            break;
        case LS_ARRAY:
            rv = delete_array_data(ctx, id);
            break;
        case LS_KV:
            rv = delete_kv_data(ctx, id);
            break;
    }

    return rv;
}

static
int update_raw_data(litestore* ctx,
                    const litestore_id_t id, const int old_type, void* data)
{
    int rv = LITESTORE_OK;

    switch (old_type)
    {
        case LS_ARRAY:
            rv = delete_array_data(ctx, id);
            break;
        case LS_KV:
            rv = delete_kv_data(ctx, id);
            break;
    }
    if (rv == LITESTORE_OK)
    {
        litestore_blob_t* value = (litestore_blob_t*)data;

        /* try update, if it fails save */
        if (sqlite3_bind_blob(ctx->update_raw_data, 1,
                              value->data, value->size,
                              SQLITE_STATIC) == SQLITE_OK
            && sqlite3_bind_int64(ctx->update_raw_data, 2, id) == SQLITE_OK)
        {
            if (sqlite3_step(ctx->update_raw_data) == SQLITE_DONE)
            {
                if (sqlite3_changes(ctx->db) == 0)
                {
                    rv = save_raw_data(ctx, id, value);
                }
            }
            else
            {
                print_sqlite_error(ctx);
                rv = LITESTORE_ERR;
            }
        }
        else
        {
            print_sqlite_error(ctx);
            rv = LITESTORE_ERR;
        }
        sqlite3_reset(ctx->update_raw_data);
    }

    return rv;
}

static
int update_array(litestore* ctx, const litestore_id_t id,
                 const unsigned index,
                 const void* value, const size_t value_len)
{
    int rv = LITESTORE_ERR;

    if (value && value_len > 0)
    {
        if (sqlite3_bind_blob(ctx->update_array_data, 1, value, value_len,
                              SQLITE_STATIC) == SQLITE_OK
            && sqlite3_bind_int64(ctx->update_array_data, 2, id) == SQLITE_OK
            && sqlite3_bind_int64(ctx->update_array_data, 3, index) == SQLITE_OK
            && sqlite3_step(ctx->update_array_data) == SQLITE_DONE)
        {
            rv = LITESTORE_OK;
            if (sqlite3_changes(ctx->db) == 0)
            {
                rv = save_array(ctx, id, index, value, value_len);
            }
        }
        else
        {
            print_sqlite_error(ctx);
        }
        sqlite3_reset(ctx->update_array_data);
    }

    return rv;
}

static
int update_array_data(litestore* ctx,
                      const litestore_id_t id, const int old_type,
                      void* data)
{
    int rv = LITESTORE_OK;

    switch (old_type)
    {
        case LS_RAW:
            rv = delete_raw_data(ctx, id);
            break;
        case LS_KV:
            rv = delete_kv_data(ctx, id);
            break;
    }
    if (rv == LITESTORE_OK)
    {
        litestore_array_iterator* iter = (litestore_array_iterator*)data;
        unsigned index = 0;
        const void* v = NULL;
        size_t v_len = 0;

        for (iter->begin(iter->user_data);
             !iter->end(iter->user_data);
             iter->next(iter->user_data), ++index)
        {
            iter->value(iter->user_data, &v, &v_len);
            if (update_array(ctx, id, index, v, v_len) != LITESTORE_OK)
            {
                rv = LITESTORE_ERR;
                break;
            }
            v = NULL;
            v_len = 0;
        }
    }

    return rv;
}

static
int update_kv(litestore* ctx, const litestore_id_t id,
              const void* key, const size_t key_len,
              const void* value, const size_t value_len)
{
    int rv = LITESTORE_ERR;

    if (key && key_len > 0 && value && value_len > 0)
    {
        if (sqlite3_bind_blob(ctx->update_kv_data, 1, value, value_len,
                              SQLITE_STATIC) == SQLITE_OK
            && sqlite3_bind_int64(ctx->update_kv_data, 2, id) == SQLITE_OK
            && sqlite3_bind_blob(ctx->update_kv_data, 3, key, key_len,
                                 SQLITE_STATIC) == SQLITE_OK
            && sqlite3_step(ctx->update_kv_data) == SQLITE_DONE)
        {
            rv = LITESTORE_OK;
            if (sqlite3_changes(ctx->db) == 0)
            {
                rv = save_kv(ctx, id, key, key_len, value, value_len);
            }
        }
        else
        {
            print_sqlite_error(ctx);
        }
        sqlite3_reset(ctx->update_kv_data);
    }

    return rv;
}

static
int update_kv_data(litestore* ctx,
                   const litestore_id_t id, const int old_type,
                   void* data)
{
    int rv = LITESTORE_OK;

    switch (old_type)
    {
        case LS_RAW:
            rv = delete_raw_data(ctx, id);
            break;
        case LS_ARRAY:
            rv = delete_array_data(ctx, id);
            break;
    }
    if (rv == LITESTORE_OK)
    {
        litestore_kv_iterator* iter = (litestore_kv_iterator*)data;
        const void* k = NULL;
        size_t k_len = 0;
        const void* v = NULL;
        size_t v_len = 0;

        for (iter->begin(iter->user_data);
             !iter->end(iter->user_data);
             iter->next(iter->user_data))
        {
            iter->value(iter->user_data, &k, &k_len, &v, &v_len);
            if (update_kv(ctx, id, k, k_len, v, v_len) != LITESTORE_OK)
            {
                rv = LITESTORE_ERR;
                break;
            }
            k = NULL;
            k_len = 0;
            v = NULL;
            v_len = 0;
        }
    }

    return rv;
}

typedef struct
{
    int object_type;
    int (*update)(litestore*, const litestore_id_t, const int, void*);
    int (*save)(litestore*, litestore_id_t, void*);
    void* data;
} update_ctx;

static
int gen_update(litestore* ctx,
               const char* key, const size_t key_len,
               update_ctx op)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0 && op.update && op.data)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int old_type = -1;
        rv = get_object_type(ctx, key, key_len, &id, &old_type);
        if (rv == LITESTORE_OK)
        {
            rv = (*op.update)(ctx, id, old_type, op.data);
            if (rv == LITESTORE_OK && old_type != op.object_type)
            {
                rv = update_object_type(ctx, id, op.object_type);
            }
        }
        else if (rv == LITESTORE_UNKNOWN_ENTITY)
        {
            save_ctx s = {op.object_type, op.save, op.data};
            rv = gen_save(ctx, key, key_len, s);
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

static inline
int slice_valid(const litestore_slice_t s)
{
    return s.data && s.length > 0;
}

static inline
int blob_valid(const litestore_blob_t b)
{
    return b.data && b.size > 0;
}

/* from litestore_helpers.h */
litestore_blob_t litestore_make_blob(const void* data, const size_t size)
{
    litestore_blob_t tmp = {data, size};
    return tmp;
}

/* from litestore_helpers.h */
litestore_slice_t litestore_slice(const char* str,
                                  const size_t begin, const size_t end)
{
    if (str && end > begin)
    {
        litestore_slice_t tmp = {str + begin, end - begin};
        return tmp;
    }
    litestore_slice_t tmp = {NULL, 0};
    return tmp;
}

/* from litestore_helpers.h */
litestore_slice_t litestore_slice_str(const char* str)
{
    return litestore_slice(str, 0, strlen(str));
}

/*-----------------------------------------*/
/*------------------ API ------------------*/
/*-----------------------------------------*/

void* litestore_native_ctx(litestore* ctx)
{
    return ctx->db;
}

/*-----------------------------------------*/
/*---------------- construction -----------*/
/*-----------------------------------------*/
int litestore_open(const char* db_file_name, litestore** ctx)
{
    *ctx = (litestore*)malloc(sizeof(litestore));
    if (*ctx)
    {
        memset(*ctx, 0, sizeof(litestore));

        if (sqlite3_open(db_file_name, &(*ctx)->db) != SQLITE_OK
            || init_db(*ctx) != LITESTORE_OK)
        {
            litestore_close(*ctx);
            *ctx = NULL;
            return LITESTORE_ERR;
        }
        return prepare_statements(*ctx);
    }
    return LITESTORE_ERR;
}

void litestore_close(litestore* ctx)
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

/*-----------------------------------------*/
/*---------------- tx ---------------------*/
/*-----------------------------------------*/
int litestore_begin_tx(litestore* ctx)
{
    const int rv = run_stmt(ctx, ctx->begin_tx);
    if (rv == LITESTORE_OK)
    {
        ctx->tx_active = 1;
    }
    return rv;
}

int litestore_commit_tx(litestore* ctx)
{
    const int rv = run_stmt(ctx, ctx->commit_tx);
    if (rv == LITESTORE_OK)
    {
        ctx->tx_active = 0;
    }
    return rv;
}

int litestore_rollback_tx(litestore* ctx)
{
    const int rv = run_stmt(ctx, ctx->rollback_tx);
    if (rv == LITESTORE_OK)
    {
        ctx->tx_active = 0;
    }
    return rv;
}

/*-----------------------------------------*/
/*---------------- null -------------------*/
/*-----------------------------------------*/
int litestore_save_null(litestore* ctx, litestore_slice_t key)
{
    int rv = LITESTORE_ERR;

    if (ctx && slice_valid(key))
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t new_id = 0;
        rv = save_key(ctx, key.data, key.length, LS_NULL, &new_id);

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

int litestore_get_null(litestore* ctx, litestore_slice_t key)
{
    int rv = LITESTORE_ERR;

    if (ctx && slice_valid(key))
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int type = -1;
        rv = get_object_type(ctx, key.data, key.length, &id, &type);

        if (rv == LITESTORE_OK && type != LS_NULL)
        {
            rv = LITESTORE_ERR;
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

int litestore_update_null(litestore* ctx, litestore_slice_t key)
{
    int rv = LITESTORE_ERR;
    if (ctx && slice_valid(key))
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int old_type = -1;
        rv = get_object_type(ctx, key.data, key.length, &id, &old_type);
        if (rv == LITESTORE_OK)
        {
            rv = update_null(ctx, id, old_type);
            if (rv == LITESTORE_OK && old_type != LS_NULL)
            {
                rv = update_object_type(ctx, id, LS_NULL);
            }
        }
        else if (rv == LITESTORE_UNKNOWN_ENTITY)
        {
            rv = litestore_save_null(ctx, key);
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }
    return rv;
}

/*-----------------------------------------*/
/*---------------- raw --------------------*/
/*-----------------------------------------*/
int litestore_save_raw(litestore* ctx,
                       litestore_slice_t key,
                       litestore_blob_t value)
{
    save_ctx op = {LS_RAW, &save_raw_data, &value};
    return gen_save(ctx, key.data, key.length, op);
}

int litestore_get_raw(litestore* ctx, litestore_slice_t key,
                      litestore_get_raw_cb callback, void* user_data)
{
    get_ctx op = {LS_RAW, &get_raw_data, callback, user_data};
    return gen_get(ctx, key.data, key.length, op);
}

int litestore_update_raw(litestore* ctx,
                         litestore_slice_t key,
                         litestore_blob_t value)
{
    update_ctx op = {LS_RAW, &update_raw_data, &save_raw_data, &value};
    return gen_update(ctx, key.data, key.length, op);
}

/*-----------------------------------------*/
/*---------------- array ------------------*/
/*-----------------------------------------*/
int litestore_save_array(litestore* ctx, litestore_slice_t key,
                         litestore_array_iterator data)
{
    save_ctx op = {LS_ARRAY, &save_array_data, &data};
    return gen_save(ctx, key.data, key.length, op);
}

int litestore_get_array(litestore* ctx, litestore_slice_t key,
                        litestore_get_array_cb callback, void* user_data)
{
    get_ctx op = {LS_ARRAY, &get_array_data, callback, user_data};
    return gen_get(ctx, key.data, key.length, op);
}

int litestore_update_array(litestore* ctx, litestore_slice_t key,
                           litestore_array_iterator data)
{
    update_ctx op = {LS_ARRAY, &update_array_data, &save_array_data, &data};
    return gen_update(ctx, key.data, key.length, op);
}

/*-----------------------------------------*/
/*---------------- kv ---------------------*/
/*-----------------------------------------*/
int litestore_save_kv(litestore* ctx, litestore_slice_t key,
                      litestore_kv_iterator data)
{
    save_ctx op = {LS_KV, &save_kv_data, &data};
    return gen_save(ctx, key.data, key.length, op);
}

int litestore_get_kv(litestore* ctx, litestore_slice_t key,
                     litestore_get_kv_cb callback, void* user_data)
{
    get_ctx op = {LS_KV, &get_kv_data, callback, user_data};
    return gen_get(ctx, key.data, key.length, op);
}

int litestore_update_kv(litestore* ctx, litestore_slice_t key,
                        litestore_kv_iterator data)
{
    update_ctx op = {LS_KV, &update_kv_data, &save_kv_data, &data};
    return gen_update(ctx, key.data, key.length, op);
}

/*-----------------------------------------*/
/*---------------- delete -----------------*/
/*-----------------------------------------*/
int litestore_delete(litestore* ctx, litestore_slice_t key)
{
    int rv = LITESTORE_ERR;

    /* delete should cascade */
    if (ctx && slice_valid(key) && ctx->delete_key)
    {
        int own_tx = opt_begin_tx(ctx);

        sqlite3_reset(ctx->delete_key);
        if (sqlite3_bind_text(ctx->delete_key,
                              1, key.data, key.length,
                              SQLITE_STATIC) == SQLITE_OK)
        {
            if (sqlite3_step(ctx->delete_key) == SQLITE_DONE)
            {
                rv = (sqlite3_changes(ctx->db) == 1 ?
                      LITESTORE_OK : LITESTORE_UNKNOWN_ENTITY);
            }
            else
            {
                print_sqlite_error(ctx);
                rv = LITESTORE_ERR;
            }
        }
        else
        {
            print_sqlite_error(ctx);
            rv = LITESTORE_ERR;
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

#ifdef __cplusplus
}  // extern "C"
#endif
