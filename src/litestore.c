/**
 * Copyright (c) 2014 Markku Linnoskivi
 *
 * See the file LICENSE.txt for copying permission.
 */
#include "litestore/litestore.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3.h>


#ifdef __cplusplus
extern "C" {
#endif

#define UNUSED(x) (void)(x)

/* Current schema version */
#define LITESTORE_CURRENT_VERSION 1

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

/**
 * The LiteStore object.
 */
struct litestore
{
litestore_opts opts;
sqlite3* db;
/* tx */
sqlite3_stmt* begin_tx;
sqlite3_stmt* commit_tx;
sqlite3_stmt* rollback_tx;
int tx_active;
/* object */
sqlite3_stmt* create_key;
sqlite3_stmt* read_key;
sqlite3_stmt* delete_key;
sqlite3_stmt* update_type;
sqlite3_stmt* read_keys;
/* raw */
sqlite3_stmt* create_raw_data;
sqlite3_stmt* read_raw_data;
sqlite3_stmt* update_raw_data;
sqlite3_stmt* delete_raw_data;
};

/* Possible db.objects.type values */
enum
{
    LS_NULL = LITESTORE_NULL_T,
    LS_RAW = LITESTORE_RAW_T
};

/* The native db ID type */
typedef sqlite3_int64 litestore_id_t;


/*-----------------------------------------*/
/*----------------- HELPERS ---------------*/
/*-----------------------------------------*/
static
void sqlite_error(litestore* ctx)
{
    if (ctx->opts.error_callback)
    {
        (*ctx->opts.error_callback)(sqlite3_errcode(ctx->db),
                                    sqlite3_errmsg(ctx->db),
                                    ctx->opts.err_user_data);
    }
    else
    {
        printf("ERROR: %s\n", sqlite3_errmsg(ctx->db));
    }
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
            sqlite_error(ctx);
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
                     &(ctx->create_key)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "SELECT id, type FROM objects WHERE name = ?;",
                        &(ctx->read_key)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "DELETE FROM objects WHERE name = ?;",
                        &(ctx->delete_key)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "UPDATE objects SET type = ? WHERE id = ?;",
                        &(ctx->update_type)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "SELECT name, type FROM objects WHERE name GLOB ?;",
                        &(ctx->read_keys)) != LITESTORE_OK
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
                        &(ctx->create_raw_data)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "SELECT raw_value FROM raw_data WHERE id = ?;",
                        &(ctx->read_raw_data)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "UPDATE raw_data SET raw_value = ? WHERE id = ?;",
                        &(ctx->update_raw_data)) != LITESTORE_OK
        || prepare_stmt(ctx,
                        "DELETE FROM raw_data WHERE id = ?;",
                        &(ctx->delete_raw_data)) != LITESTORE_OK)
    {
        sqlite_error(ctx);
        return LITESTORE_ERR;
    }

    return LITESTORE_OK;
}

static
int update_version_from(litestore* ctx, int version_in_db)
{
    int rv = LITESTORE_OK;

    while (version_in_db != LITESTORE_CURRENT_VERSION && rv == LITESTORE_OK)
    {
        switch (version_in_db)
        {
            case 0:
            {
                const char* stmt =
                    "INSERT INTO meta (schema_version) VALUES (?);";
                sqlite3_stmt* save_version = NULL;
                if (sqlite3_prepare_v2(ctx->db, stmt, strlen(stmt),
                                       &save_version, NULL)
                    == SQLITE_OK
                    && sqlite3_bind_int(
                        save_version, 1, LITESTORE_CURRENT_VERSION)
                    == SQLITE_OK
                    && sqlite3_step(save_version)
                    == SQLITE_DONE)
                {
                    version_in_db = LITESTORE_CURRENT_VERSION;
                }
                else
                {
                    sqlite_error(ctx);
                    rv = LITESTORE_ERR;
                }
                sqlite3_finalize(save_version);
            }
            break;

            default:
                rv = LITESTORE_UNSUPPORTED_VERSION;
                break;
        }
    }

    return rv;
}

static
int version_update(litestore* ctx)
{
    int rv = LITESTORE_ERR;

    if (opt_begin_tx(ctx))
    {
        sqlite3_stmt* read_version = NULL;
        const char* stmt = "SELECT schema_version FROM meta;";
        if (sqlite3_prepare_v2(ctx->db, stmt, strlen(stmt), &read_version, NULL)
            == SQLITE_OK)
        {
            if (sqlite3_step(read_version) == SQLITE_ROW)
            {
                const int version = sqlite3_column_int(read_version, 0);
                if (version < LITESTORE_CURRENT_VERSION)
                {
                    rv = update_version_from(ctx, version);
                }
                else if (version > LITESTORE_CURRENT_VERSION)
                {
                    rv = LITESTORE_UNSUPPORTED_VERSION;
                }
            }
            else
            {
                rv = update_version_from(ctx, 0);
            }
            sqlite3_finalize(read_version);
        }
        else
        {
            sqlite_error(ctx);
        }

        rv = opt_end_tx(ctx, rv);
    }

    return rv;
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
    finalize_stmt(&(ctx->create_key));
    finalize_stmt(&(ctx->read_key));
    finalize_stmt(&(ctx->delete_key));
    finalize_stmt(&(ctx->update_type));
    finalize_stmt(&(ctx->read_keys));
    /* tx */
    finalize_stmt(&(ctx->begin_tx));
    finalize_stmt(&(ctx->commit_tx));
    finalize_stmt(&(ctx->rollback_tx));
    /* raw */
    finalize_stmt(&(ctx->create_raw_data));
    finalize_stmt(&(ctx->read_raw_data));
    finalize_stmt(&(ctx->update_raw_data));
    finalize_stmt(&(ctx->delete_raw_data));
    
    return LITESTORE_OK;
}


/*-----------------------------------------*/
/*----------------- CREATE ----------------*/
/*-----------------------------------------*/
static
int create_key(litestore* ctx,
               const char* key, const size_t key_len,
               const int data_type,
               litestore_id_t* id)
{
    if (ctx->create_key)
    {
        sqlite3_reset(ctx->create_key);
        if (sqlite3_bind_text(ctx->create_key,
                              1, key, key_len,
                              SQLITE_STATIC) != SQLITE_OK
            || sqlite3_bind_int(ctx->create_key,
                                2, data_type) != SQLITE_OK)
        {
            sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        if (sqlite3_step(ctx->create_key) != SQLITE_DONE)
        {
            sqlite_error(ctx);
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
int create_raw_data(litestore* ctx, litestore_id_t new_id, void* value)
{
    int rv = LITESTORE_ERR;

    litestore_blob_t* blob = (litestore_blob_t*)value;
    if (ctx->create_raw_data && blob->data && blob->size > 0)
    {
        if (sqlite3_bind_int64(ctx->create_raw_data, 1, new_id) == SQLITE_OK
            && sqlite3_bind_blob(ctx->create_raw_data,
                                 2, blob->data, blob->size,
                                 SQLITE_STATIC) == SQLITE_OK
            && sqlite3_step(ctx->create_raw_data) == SQLITE_DONE)
        {
            rv = LITESTORE_OK;
        }
        else
        {
            sqlite_error(ctx);
        }
        sqlite3_reset(ctx->create_raw_data);
    }
    return rv;
}

typedef struct
{
    int object_type;
    int (*create)(litestore*, litestore_id_t, void*);
    void* data;
} create_ctx;

static
int gen_create(litestore* ctx,
               const char* key, const size_t key_len,
               create_ctx op)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0 && op.create && op.data)
    {
        const int own_tx = opt_begin_tx(ctx);

        litestore_id_t new_id = 0;
        rv = create_key(ctx, key, key_len, op.object_type, &new_id);
        if (rv == LITESTORE_OK)
        {
            rv = (*op.create)(ctx, new_id, op.data);
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

/*-----------------------------------------*/
/*----------------- READ ------------------*/
/*-----------------------------------------*/
static
int read_object_type(litestore* ctx,
                     const char* key, const size_t key_len,
                     litestore_id_t* id, int* type)
{
    if (ctx->read_key && key && key_len > 0)
    {
        sqlite3_reset(ctx->read_key);
        if (sqlite3_bind_text(ctx->read_key,
                              1, key, key_len,
                              SQLITE_STATIC) != SQLITE_OK)
        {
            sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        /* expect only one aswer */
        if (sqlite3_step(ctx->read_key) != SQLITE_ROW)
        {
            *id = 0;
            *type = -1;
            return LITESTORE_UNKNOWN_ENTITY;
        }
        *id = sqlite3_column_int64(ctx->read_key, 0);
        *type = sqlite3_column_int(ctx->read_key, 1);
        return LITESTORE_OK;
    }
    return LITESTORE_ERR;
}

static
int read_raw_data(litestore* ctx, const litestore_id_t id,
                  const void* key, const size_t key_len,
                  void* extra, void* cb, void* user_data)
{
    UNUSED(key);
    UNUSED(key_len);
    UNUSED(extra);
    int rv = LITESTORE_ERR;

    if (ctx->read_raw_data && cb)
    {
        litestore_read_raw_cb callback = (litestore_read_raw_cb)cb;

        if (sqlite3_bind_int64(ctx->read_raw_data, 1, id) == SQLITE_OK
            && sqlite3_step(ctx->read_raw_data) == SQLITE_ROW)
        {
            const void* raw_data =
                sqlite3_column_blob(ctx->read_raw_data, 0);
            const int bytes =
                sqlite3_column_bytes(ctx->read_raw_data, 0);
            if (bytes > 0)
            {
                rv = (*callback)(
                    litestore_make_blob(raw_data, bytes), user_data);
            }
        }
        else
        {
            sqlite_error(ctx);
        }
        sqlite3_reset(ctx->read_raw_data);
    }

    return rv;
}

typedef struct
{
    int object_type;
    int (*read)(litestore*, const litestore_id_t,
                const void* /* key */, const size_t /* key_len */,
                void* /* extra */, void* /* cb */, void* /* user_data */);
    void* extra;
    void* callback;
    void* user_data;
} read_ctx;

static
int gen_read(litestore* ctx,
             const char* key, const size_t key_len,
             read_ctx op)
{
    int rv = LITESTORE_ERR;

    if (ctx && key && key_len > 0 && op.read)
    {
        const int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int type = -1;
        rv = read_object_type(ctx, key, key_len, &id, &type);

        if (rv == LITESTORE_OK && type == op.object_type)
        {
            rv = (*op.read)(ctx, id, key, key_len, op.extra,
                            op.callback, op.user_data);
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
            sqlite_error(ctx);
            return LITESTORE_ERR;
        }
        if (sqlite3_step(ctx->delete_raw_data) != SQLITE_DONE)
        {
            sqlite_error(ctx);
            return LITESTORE_ERR;
        }
    }
    else
    {
        return LITESTORE_ERR;
    }
    return LITESTORE_OK;
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
        sqlite_error(ctx);
        return LITESTORE_ERR;
    }
    if (sqlite3_step(ctx->update_type) != SQLITE_DONE)
    {
        sqlite_error(ctx);
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
    }

    return rv;
}

static
int update_raw_data(litestore* ctx,
                    const litestore_id_t id, const int old_type, void* data)
{
    UNUSED(old_type);
    
    int rv = LITESTORE_OK;

    if (rv == LITESTORE_OK)
    {
        litestore_blob_t* value = (litestore_blob_t*)data;

        /* try update, if it fails create */
        if (sqlite3_bind_blob(ctx->update_raw_data, 1,
                              value->data, value->size,
                              SQLITE_STATIC) == SQLITE_OK
            && sqlite3_bind_int64(ctx->update_raw_data, 2, id) == SQLITE_OK)
        {
            if (sqlite3_step(ctx->update_raw_data) == SQLITE_DONE)
            {
                if (sqlite3_changes(ctx->db) == 0)
                {
                    rv = create_raw_data(ctx, id, value);
                }
            }
            else
            {
                sqlite_error(ctx);
                rv = LITESTORE_ERR;
            }
        }
        else
        {
            sqlite_error(ctx);
            rv = LITESTORE_ERR;
        }
        sqlite3_reset(ctx->update_raw_data);
    }

    return rv;
}

typedef struct
{
    int object_type;
    int (*update)(litestore*, const litestore_id_t, const int, void*);
    int (*create)(litestore*, litestore_id_t, void*);
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
        const int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int old_type = -1;
        rv = read_object_type(ctx, key, key_len, &id, &old_type);
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
            create_ctx s = {op.object_type, op.create, op.data};
            rv = gen_create(ctx, key, key_len, s);
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
int litestore_open(const char* file_name,
                   litestore_opts opts,
                   litestore** ctx)
{
    *ctx = (litestore*)malloc(sizeof(litestore));
    if (*ctx)
    {
        memset(*ctx, 0, sizeof(litestore));
        (*ctx)->opts = opts;
        if (sqlite3_open(file_name, &(*ctx)->db) != SQLITE_OK
            || init_db(*ctx) != LITESTORE_OK)
        {
            litestore_close(*ctx);
            *ctx = NULL;
            return LITESTORE_ERR;
        }
        if (prepare_statements(*ctx) == LITESTORE_OK)
        {
            return version_update(*ctx);
        }
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
int litestore_create_null(litestore* ctx, litestore_slice_t key)
{
    int rv = LITESTORE_ERR;

    if (ctx && slice_valid(key))
    {
        const int own_tx = opt_begin_tx(ctx);

        litestore_id_t new_id = 0;
        rv = create_key(ctx, key.data, key.length, LS_NULL, &new_id);

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

int litestore_read_null(litestore* ctx, litestore_slice_t key)
{
    int rv = LITESTORE_ERR;

    if (ctx && slice_valid(key))
    {
        const int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int type = -1;
        rv = read_object_type(ctx, key.data, key.length, &id, &type);

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
        const int own_tx = opt_begin_tx(ctx);

        litestore_id_t id = 0;
        int old_type = -1;
        rv = read_object_type(ctx, key.data, key.length, &id, &old_type);
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
            rv = litestore_create_null(ctx, key);
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
int litestore_create_raw(litestore* ctx,
                         litestore_slice_t key,
                         litestore_blob_t value)
{
    create_ctx op = {LS_RAW, &create_raw_data, &value};
    return gen_create(ctx, key.data, key.length, op);
}

int litestore_read_raw(litestore* ctx, litestore_slice_t key,
                       litestore_read_raw_cb callback, void* user_data)
{
    read_ctx op = {LS_RAW, &read_raw_data, NULL, callback, user_data};
    return gen_read(ctx, key.data, key.length, op);
}

int litestore_update_raw(litestore* ctx,
                         litestore_slice_t key,
                         litestore_blob_t value)
{
    update_ctx op = {LS_RAW, &update_raw_data, &create_raw_data, &value};
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
        const int own_tx = opt_begin_tx(ctx);

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
                sqlite_error(ctx);
                rv = LITESTORE_ERR;
            }
        }
        else
        {
            sqlite_error(ctx);
            rv = LITESTORE_ERR;
        }

        if (own_tx)
        {
            opt_end_tx(ctx, rv);
        }
    }

    return rv;
}

int litestore_read_keys(litestore* ctx,
                        litestore_slice_t key_pattern,
                        litestore_read_keys_cb callback,
                        void* user_data)
{
    int rv = LITESTORE_ERR;

    if (ctx->read_keys && callback)
    {
        const int own_tx = opt_begin_tx(ctx);

        if (sqlite3_bind_text(ctx->read_keys, 1,
                              key_pattern.data, key_pattern.length,
                              SQLITE_STATIC)
            != SQLITE_OK)
        {
            sqlite_error(ctx);
        }
        else
        {
            int rc = 0;

            while ((rc = sqlite3_step(ctx->read_keys)) != SQLITE_DONE)
            {
                if (rc == SQLITE_ROW)
                {
                    const unsigned char* key =
                        sqlite3_column_text(ctx->read_keys, 0);
                    const int key_len =
                        sqlite3_column_bytes(ctx->read_keys, 0);
                    if ((*callback)(
                            litestore_slice((const char*)key, 0, key_len),
                            sqlite3_column_int(ctx->read_keys, 1),
                            user_data)
                        != LITESTORE_OK)
                    {
                        break;
                    }
                }
                else
                {
                    sqlite_error(ctx);
                    break;
                }
            }  /* while */
            if (rc == SQLITE_DONE)
            {
                rv = LITESTORE_OK;
            }

            sqlite3_reset(ctx->read_keys);
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
