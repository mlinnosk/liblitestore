#include "litestore.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3.h>


#ifdef __cplusplus
extern "C"
#endif


/**
 * The DB schema.
 */
#define LITESTORE_SCHEMA_V1                                \
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

void print_sqlite_error(litestore* ctx)
{
    printf("ERROR: %s\n", sqlite3_errmsg(ctx->db));
}

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

inline
int prepare_stmt(litestore* ctx, const char* sql, sqlite3_stmt** stmt)
{
    if (sqlite3_prepare_v2(ctx->db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        return LITESTORE_ERR;
    }
    return LITESTORE_OK;
}

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

inline
void finalize_stmt(sqlite3_stmt** stmt)
{
    sqlite3_finalize(*stmt);
    *stmt = NULL;
}

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

int save_raw_data(litestore* ctx,
                     const litestore_id_t new_id,
                     const void* value, const size_t value_len)
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
            print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        if (sqlite3_step(ctx->save_raw_data) != SQLITE_DONE)
        {
            print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        return LITESTORE_OK;
    }
    return LITESTORE_ERR;
}

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

int save_kv_data(litestore* ctx, const litestore_id_t new_id,
                 litestore_kv_iterator* data)
{
    const void* k = NULL;
    size_t k_len = 0;
    const void* v = NULL;
    size_t v_len = 0;

    for (data->begin(data->user_data);
         !data->end(data->user_data);
         data->next(data->user_data))
    {
        data->value(data->user_data, &k, &k_len, &v, &v_len);
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

int save_array_data(litestore* ctx, const litestore_id_t new_id,
                    litestore_array_iterator* data)
{
    unsigned index = 0;
    const void* v = NULL;
    size_t v_len = 0;

    for (data->begin(data->user_data);
         !data->end(data->user_data);
         data->next(data->user_data))
    {
        data->value(data->user_data, &v, &v_len);
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


/*-----------------------------------------*/
/*----------------- GET -------------------*/
/*-----------------------------------------*/

int get_key_type(litestore* ctx,
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

int get_raw_data(litestore* ctx,
                    const litestore_id_t id,
                    litestore_get_raw_cb callback, void* user_data)
{
    if (ctx->get_raw_data)
    {
        sqlite3_reset(ctx->get_raw_data);
        if (sqlite3_bind_int64(ctx->get_raw_data,
                               1, id) != SQLITE_OK)
        {
            print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        /* expect only one aswer */
        if (sqlite3_step(ctx->get_raw_data) != SQLITE_ROW)
        {
            print_sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        const void* raw_data = sqlite3_column_blob(ctx->get_raw_data,
                                                   0);
        const int bytes = sqlite3_column_bytes(ctx->get_raw_data,
                                               0);
        if (bytes > 0)
        {
            return (*callback)(raw_data, bytes, user_data);
        }
    }
    return LITESTORE_ERR;
}

int update_type(litestore* ctx,
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

int update_raw_data(litestore* ctx,
                       const litestore_id_t id, const int old_type,
                       const void* value, const size_t value_len)
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
        /* try update, if it fails save */
        if (sqlite3_bind_blob(ctx->update_raw_data, 1, value, value_len,
                              SQLITE_STATIC) == SQLITE_OK
            && sqlite3_bind_int64(ctx->update_raw_data, 2, id) == SQLITE_OK)
        {
            if (sqlite3_step(ctx->update_raw_data) == SQLITE_DONE)
            {
                if (sqlite3_changes(ctx->db) == 0)
                {
                    rv = save_raw_data(ctx, id, value, value_len);
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

int update_kv_data(litestore* ctx,
                   const litestore_id_t id, const int old_type,
                   litestore_kv_iterator* data)
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
        const void* k = NULL;
        size_t k_len = 0;
        const void* v = NULL;
        size_t v_len = 0;

        for (data->begin(data->user_data);
             !data->end(data->user_data);
             data->next(data->user_data))
        {
            data->value(data->user_data, &k, &k_len, &v, &v_len);
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

int update_array_data(litestore* ctx,
                      const litestore_id_t id, const int old_type,
                      litestore_array_iterator* data)
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
        unsigned index = 0;
        const void* v = NULL;
        size_t v_len = 0;

        for (data->begin(data->user_data);
             !data->end(data->user_data);
             data->next(data->user_data), ++index)
        {
            data->value(data->user_data, &v, &v_len);
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
int opt_end_tx(litestore* ctx, int status)
{
    if (status == LITESTORE_OK)
    {
        return litestore_commit_tx(ctx);
    }
    return litestore_rollback_tx(ctx);
}

int get_kv_data(litestore* ctx,
                const litestore_id_t id,
                const void* key, const size_t key_len,
                litestore_get_kv_cb callback, void* user_data)
{
    int rv = LITESTORE_ERR;

    if (ctx->get_kv_data && callback)
    {
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
                    if ((*callback)(key, key_len,
                                    k, k_len, v, v_len,
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
            }  // while
            if (rc == SQLITE_DONE)
            {
                rv = LITESTORE_OK;
            }
        }
        sqlite3_reset(ctx->get_kv_data);
    }

    return rv;
}

int get_array_data(litestore* ctx,
                   const litestore_id_t id,
                   const void* key, const size_t key_len,
                   litestore_get_array_cb callback, void* user_data)
{
    int rv = LITESTORE_ERR;

    if (ctx->get_array_data && callback)
    {
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
                    if ((*callback)(key, key_len,
                                    index, v, v_len,
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
            }  // while
            if (rc == SQLITE_DONE)
            {
                rv = LITESTORE_OK;
            }
        }
        sqlite3_reset(ctx->get_array_data);
    }

    return rv;
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
int litestore_save_null(litestore* ctx,
                        const char* key, const size_t key_len)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t new_id = 0;
        rv = save_key(ctx, key, key_len, LS_NULL, &new_id);

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

int litestore_get_null(litestore* ctx,
                       const char* key, const size_t key_len)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int type = -1;
        rv = get_key_type(ctx, key, key_len, &id, &type);

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

int litestore_update_null(litestore* ctx,
                          const char* key, const size_t key_len)
{
    int rv = LITESTORE_ERR;
    if (ctx && key && key_len > 0)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int old_type = -1;
        rv = get_key_type(ctx, key, key_len, &id, &old_type);
        if (rv == LITESTORE_OK)
        {
            rv = update_null(ctx, id, old_type);
            if (rv == LITESTORE_OK && old_type != LS_NULL)
            {
                rv = update_type(ctx, id, LS_NULL);
            }
        }
        else if (rv == LITESTORE_UNKNOWN_ENTITY)
        {
            rv = litestore_save_null(ctx, key, key_len);
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
                       const char* key, const size_t key_len,
                       const void* value, const size_t value_len)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0 && value && value_len > 0)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t new_id = 0;
        rv = save_key(ctx, key, key_len, LS_RAW, &new_id);
        if (rv == LITESTORE_OK)
        {
            rv = save_raw_data(ctx, new_id, value, value_len);
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

int litestore_get_raw(litestore* ctx,
                      const char* key, const size_t key_len,
                      litestore_get_raw_cb callback, void* user_data)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int type = -1;
        rv = get_key_type(ctx, key, key_len, &id, &type);

        if (rv == LITESTORE_OK && type == LS_RAW)
        {
            rv = get_raw_data(ctx, id, callback, user_data);
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

int litestore_update_raw(litestore* ctx,
                         const char* key, const size_t key_len,
                         const void* value, const size_t value_len)
{
    int rv = LITESTORE_ERR;
    if (ctx && key && key_len > 0)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int old_type = -1;
        rv = get_key_type(ctx, key, key_len, &id, &old_type);
        if (rv == LITESTORE_OK)
        {
            rv = update_raw_data(ctx,
                                    id, old_type,
                                    value, value_len);
            if (rv == LITESTORE_OK && old_type != LS_RAW)
            {
                rv = update_type(ctx, id, LS_RAW);
            }
        }
        else if (rv == LITESTORE_UNKNOWN_ENTITY)
        {
            rv = litestore_save_raw(ctx, key, key_len, value, value_len);
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }
    return rv;
}

/*-----------------------------------------*/
/*---------------- array ------------------*/
/*-----------------------------------------*/
int litestore_save_array(litestore* ctx,
                         const char* key, const size_t key_len,
                         litestore_array_iterator data)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t new_id = 0;
        rv = save_key(ctx, key, key_len, LS_ARRAY, &new_id);
        if (rv == LITESTORE_OK)
        {
            rv = save_array_data(ctx, new_id, &data);
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

int litestore_get_array(litestore* ctx,
                        const char* key, const size_t key_len,
                        litestore_get_array_cb callback, void* user_data)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int type = -1;
        rv = get_key_type(ctx, key, key_len, &id, &type);

        if (rv == LITESTORE_OK && type == LS_ARRAY)
        {
            rv = get_array_data(ctx, id, key, key_len, callback, user_data);
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

int litestore_update_array(litestore* ctx,
                           const char* key, const size_t key_len,
                           litestore_array_iterator data)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int old_type = -1;
        rv = get_key_type(ctx, key, key_len, &id, &old_type);
        if (rv == LITESTORE_OK)
        {
            rv = update_array_data(ctx, id, old_type, &data);
            if (rv == LITESTORE_OK && old_type != LS_ARRAY)
            {
                rv = update_type(ctx, id, LS_ARRAY);
            }
        }
        else if (rv == LITESTORE_UNKNOWN_ENTITY)
        {
            rv = litestore_save_array(ctx, key, key_len, data);
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

/*-----------------------------------------*/
/*---------------- kv ---------------------*/
/*-----------------------------------------*/
int litestore_save_kv(litestore* ctx,
                      const char* key, const size_t key_len,
                      litestore_kv_iterator data)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t new_id = 0;
        rv = save_key(ctx, key, key_len, LS_KV, &new_id);
        if (rv == LITESTORE_OK)
        {
            rv = save_kv_data(ctx, new_id, &data);
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

int litestore_get_kv(litestore* ctx,
                     const char* key, const size_t key_len,
                     litestore_get_kv_cb callback, void* user_data)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int type = -1;
        rv = get_key_type(ctx, key, key_len, &id, &type);

        if (rv == LITESTORE_OK && type == LS_KV)
        {
            rv = get_kv_data(ctx, id, key, key_len, callback, user_data);
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

int litestore_update_kv(litestore* ctx,
                        const char* key, const size_t key_len,
                        litestore_kv_iterator data)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0)
    {
        int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int old_type = -1;
        rv = get_key_type(ctx, key, key_len, &id, &old_type);
        if (rv == LITESTORE_OK)
        {
            rv = update_kv_data(ctx, id, old_type, &data);
            if (rv == LITESTORE_OK && old_type != LS_KV)
            {
                rv = update_type(ctx, id, LS_KV);
            }
        }
        else if (rv == LITESTORE_UNKNOWN_ENTITY)
        {
            rv = litestore_save_kv(ctx, key, key_len, data);
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

/*-----------------------------------------*/
/*---------------- delete -----------------*/
/*-----------------------------------------*/
int litestore_delete(litestore* ctx,
                     const char* key, const size_t key_len)
{
    int rv = LITESTORE_ERR;

    /* delete should cascade */
    if (ctx && key && key_len > 0 && ctx->delete_key)
    {
        int own_tx = opt_begin_tx(ctx);

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
