/**
 * Copyright (c) 2014 Markku Linnoskivi
 *
 * See the file LICENSE.txt for copying permission.
 */
#ifndef LITESTORE_LITESTORE_H
#define LITESTORE_LITESTORE_H

#include <stddef.h>

#include "litestore_helpers.h"
#include "litestore_iterator.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * The Litestore handle type.
 */
typedef struct litestore litestore;

/**
 * Return values.
 */
enum
{
    LITESTORE_OK = 0,
    LITESTORE_ERR = -1,
    LITESTORE_UNKNOWN_ENTITY = -2
};

/**
 * @return The actual context of the DB. To facilitate testing.
 */
void* litestore_native_ctx(litestore* ctx);

/**
 * Callback for errors.
 *
 * @param error The error code.
 * @param desc Information about the error.
 * @param user_data
 */
typedef void (*litestore_error)(const int error, const char* desc,
                                void* user_data);
/**
 * Structure used to pass data to open.
 */
typedef struct
{
    litestore_error error_callback; /* called on internal (sql) errors */
    void* err_user_data;  /* passed to error_callback */
} litestore_opts;
/**
 * Open a connection to the store in db_file_name.
 *
 * If the store does not exist, it will be created.
 * Multipple connection can be open at the same time.
 * @see For threading: http://www.sqlite.org/threadsafe.html
 *
 * @param file_name A null terminated UTF-8 encoded c-string,
 *                  pointing to the actual database.
 * @param opts Option structure.
 * @param ctx A pointer to a Litestore context structure that will allocated.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_open(const char* file_name,
                   litestore_opts opts,
                   litestore** ctx);
/**
 * Close the connection to the store.
 *
 * @param ctx The context allocated by litestore_open.
 */
void litestore_close(litestore* ctx);
/**
 * Begin transaction.
 *
 * Multipple API calls can be wrapped inside a single transaction using
 * this function. Transaction must be ended using 'commit_tx' oR 'rollback_tx'.
 *
 * Without using explicit transactions, each API call will run in it's own
 * transaction.
 *
 * @ctx The context allocated by litestore_open.
 */
int litestore_begin_tx(litestore* ctx);
/**
 * Commit transaction.
 *
 * All the changes will take permanent effect (stored) after this call.
 *
 * @ctx The context allocated by litestore_open.
 */
int litestore_commit_tx(litestore* ctx);
/**
 * Rollback transaction.
 *
 * Can be called instead of 'commit_tx' to rollback all actions done since
 * the call to 'begin_tx'.
 *
 * @ctx The context allocated by litestore_open.
 */
int litestore_rollback_tx(litestore* ctx);
/**
 * Save 'null' value int the store.
 * Inserts the given key in the store.
 * Will fail if key exists.
 *
 * @param ctx
 * @param key The key.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_save_null(litestore* ctx, litestore_slice_t key);
/**
 * Get 'null' value.
 * Checks for existance, since value is null.
 *
 * @param ctx
 * @param key The key.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_get_null(litestore* ctx, litestore_slice_t key);
/**
 * Update an object to have type of 'null'.
 * Efectively will delete other data if the the type is other than null.
 * If key is not found, it will be created.
 *
 * @param ctx
 * @param key The key.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_update_null(litestore* ctx, litestore_slice_t key);
/**
 * Save 'raw' value int the store.
 * Associate the given (new) key with the given value.
 * Will fail if key exists.
 *
 * Will NOT accept NULL data.
 *
 * @param ctx
 * @param key The key.
 * @param value The value.
 * @return LITESTORE_OK on success
 *         LITESTORE_ERR on error.
 */
int litestore_save_raw(litestore* ctx,
                       litestore_slice_t key,
                       litestore_blob_t value);
/**
 * A callback to be used with get_raw.
 *
 * @param value The data read.
 * @param user_data User provided data.
 * @return On success LITESTORE_OK, user defined otherwise.
 */
typedef int (*litestore_get_raw_cb)(litestore_blob_t value, void* user_data);
/**
 * Get a 'raw' value with the given key.
 *
 * @param ctx
 * @param key The key.
 * @param callback A callback that will be called for the read value.
 * @param user_data User provided data passed to the callback.
 *
 * @return LITESTORE_OK on success,
 *         Callback return if other than LITESTORE_OK,
 *         LITESTORE_ERR otherwise.
 */
int litestore_get_raw(litestore* ctx, litestore_slice_t key,
                      litestore_get_raw_cb callback, void* user_data);
/**
 * Update existing value with new 'raw' data.
 * If the key does not exist, it will be created.
 *
 * If the old type is other than 'raw' the data will be deleted.
 *
 * @param ctx
 * @param key The key.
 * @param value The value.
 * @return LITESTORE_OK on success
 *         LITESTORE_ERR on error.
 */
int litestore_update_raw(litestore* ctx,
                         litestore_slice_t key,
                         litestore_blob_t value);
/**
 * Save array-object in the store.
 * Associate all values described by 'data' iterator
 * to the given key.
 * Will fail if key exists.
 *
 * @param ctx
 * @param key The key.
 * @param values Iterator for array data.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_save_array(litestore* ctx,
                         litestore_slice_t key,
                         litestore_array_iterator values);

/**
 * Callback used with get_array.
 *
 * @param key The key.
 * @param array_index Index of the value that was stored.
 * @param array_value The value at index.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
typedef int (*litestore_get_array_cb)(litestore_slice_t key,
                                      unsigned array_index,
                                      litestore_blob_t value,
                                      void* user_data);
/**
 * Get an array-object with the given key.
 * The given callback will be called for each value kv pair.
 * Values will be retrieved in the same order they were stored.
 *
 * @param ctx
 * @param key The key.
 * @param callback A Function pointer to a callback called for each value
 * @param user_data Pointer to user data passed for callback calls.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_get_array(litestore* ctx,
                        litestore_slice_t key,
                        litestore_get_array_cb callback, void* user_data);
/**
 * Update existing value with new 'array' data.
 * If the key does not exist, it will be created.
 *
 * If the old type is other than 'array' the data will be deleted.
 *
 * @note If the object exists, only the array indexes listed
 *       by 'data' will be updated. Possibly existing indexes that are
 *       not listed by 'data' will _not_ be deleted. For some
 *       this might be counter intuitive. Reasoning behind this
 *       is performance and simplicity.
 *       If one needs to delete, use delete and then save.
 *       Updates are "incremental".
 *
 * @param ctx
 * @param key The key.
 * @param values Iterator for the array data.
 * @return LITESTORE_OK on success
 *         LITESTORE_ERR on error.
 */
int litestore_update_array(litestore* ctx,
                           litestore_slice_t key,
                           litestore_array_iterator values);
/**
 * Save 'key-value'-object in the store.
 * Associate all key-value pairs described by 'data' iterator
 * to the given key.
 * Will fail if key exists.
 *
 * @param ctx
 * @param key The key.
 * @param values Iterator for the key-value data.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_save_kv(litestore* ctx,
                      litestore_slice_t key,
                      litestore_kv_iterator values);
/**
 * Callback used with get_kv.
 *
 * @param key The key.
 * @param key_len Length of the key.
 * @param kv_key The key part of value.
 * @param kv_key_len
 * @param kv_value The value part of value.
 * @param kv_value_len
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
typedef int (*litestore_get_kv_cb)(litestore_slice_t key,
                                   litestore_blob_t kv_key,
                                   litestore_blob_t kv_value,
                                   void* user_data);
/**
 * Get a 'key-value'-object with the given key.
 * The given callback will be called for each value kv pair.
 *
 * @param ctx
 * @param key The key.
 * @param callback A Function pointer to a callbeck called for each
 *                 value pair.
 * @param user_data Pointer to user data passed for callback calls.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_get_kv(litestore* ctx,
                     litestore_slice_t key,
                     litestore_get_kv_cb callback, void* user_data);
/**
 * Update existing value with new 'key-value' data.
 * If the key does not exist, it will be created.
 *
 * If the old type is other than 'kv' the data will be deleted.
 *
 * @note If the object exists, only the key-value pairs listed
 *       by 'data' will be updated. Possibly existing KVs that are
 *       not listed by 'data' will _not_ be deleted. For some
 *       this might be counter intuitive. Reasoning behind this
 *       is performance and simplicity. One can change only
 *       one value of a key-value pair without affecting the others.
 *       If one needs to delete, use delete_kv and then save.
 *       Updates are "incremental".
 *
 * @param ctx
 * @param key The key.
 * @param values Iterator for the key-value data.
 * @return LITESTORE_OK on success
 *         LITESTORE_ERR on error.
 */
int litestore_update_kv(litestore* ctx,
                        litestore_slice_t key,
                        litestore_kv_iterator values);
/**
 * Delete the given entry from the store.
 * Deletes all types.
 *
 * @param ctx
 * @param key The key.
 * @return LITESTORE_OK on success,
 *         LITESTORE_UNKNOWN_ENTITY if key is not found,
 *         LITESTORE_ERR on other error.
 */
int litestore_delete(litestore* ctx, litestore_slice_t key);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* LITESTORE_LITESTORE_H */
