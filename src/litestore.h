#ifndef LITESTORE_LITESTORE_H
#define LITESTORE_LITESTORE_H

#include <stddef.h>

#include "litestore_iterator.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct litestore litestore;

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

int litestore_open(const char* db_file_name, litestore** ctx);
void litestore_close(litestore* ctx);

int litestore_begin_tx(litestore* ctx);
int litestore_commit_tx(litestore* ctx);
int litestore_rollback_tx(litestore* ctx);

/**
 * Save 'null' value int the store.
 * Inserts the given key in the store.
 * Will fail if key exists.
 *
 * @param ctx
 * @param key The key.
 * @param key_len Length of the key.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_save_null(litestore* ctx,
                        const char* key, const size_t key_len);
/**
 * Get 'null' value.
 * Checks for existance, since value is null.
 *
 * @param ctx
 * @param key The key.
 * @param key_len Length of the key.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_get_null(litestore* ctx,
                       const char* key, const size_t key_len);
/**
 * Update an object to have type of 'null'.
 * Efectively will delete other data if the the type is other than null.
 * If key is not found, it will be created.
 *
 * @param ctx
 * @param key The key.
 * @param key_len Length of the key.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_update_null(litestore* ctx,
                          const char* key, const size_t key_len);
/**
 * Save 'raw' value int the store.
 * Associate the given (new) key with the given value.
 * Will fail if key exists.
 *
 * Will NOT accept NULL data.
 *
 * @param ctx
 * @param key The key.
 * @param key_len Length of the key, excluding null terminator.
 * @param value The value.
 * @param value_len Length of the value in bytes.
 * @return LITESTORE_OK on success
 *         LITESTORE_ERR on error.
 */
int litestore_save_raw(litestore* ctx,
                       const char* key, const size_t key_len,
                       const void* value, const size_t value_len);
/**
 * A callback to be used with get_raw.
 *
 * @param value The data read.
 * @param value_len The length of the value in bytes.
 * @param user_data User provided data.
 * @return On success LITESTORE_OK, user defined otherwise.
 */
typedef int (*litestore_get_raw_cb)(const void* data, size_t data_len,
                                    void* user_data);
/**
 * Get a 'raw' value with the given key.
 *
 * @param ctx
 * @param key The key.
 * @param key_len Length of the key.
 * @param callback A callback that will be called for the read value.
 * @param user_data User provided data passed to the callback.
 *
 * @return LITESTORE_OK on success,
 *         Callback return if other than LITESTORE_OK,
 *         LITESTORE_ERR otherwise.
 */
int litestore_get_raw(litestore* ctx,
                      const char* key, const size_t key_len,
                      litestore_get_raw_cb callback, void* user_data);
/**
 * Update existing value with new 'raw' data.
 * If the key does not exist, it will be created.
 *
 * If the old type is other than 'raw' the data will be deleted.
 *
 * @param ctx
 * @param key The key.
 * @param key_len Length of the key, excluding null terminator.
 * @param value The value.
 * @param value_len Length of the value in bytes.
 * @return LITESTORE_OK on success
 *         LITESTORE_ERR on error.
 */
int litestore_update_raw(litestore* ctx,
                         const char* key, const size_t key_len,
                         const void* value, const size_t value_len);
/**
 * Save array-object in the store.
 * Associate all values described by 'data' iterator
 * to the given key.
 * Will fail if key exists.
 *
 * @param ctx
 * @param key The key.
 * @param key_len Length of the key.
 * @param data Iterator for array data.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_save_array(litestore* ctx,
                         const char* key, const size_t key_len,
                         litestore_array_iterator data);

/**
 * Callback used with get_array.
 *
 * @param key The key.
 * @param key_len Length of the key.
 * @param array_index Index of the value that was stored.
 * @param array_value The value at index.
 * @param array_value_len Length of the value.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
typedef int (*litestore_get_array_cb)(
    const char* key, const size_t key_len,
    const unsigned array_index,
    const void* array_value, const size_t array_value_len,
    void* user_data);
/**
 * Get an array-object with the given key.
 * The given callback will be called for each value kv pair.
 * Values will be retrieved in the same order they were stored.
 *
 * @param ctx
 * @param key The key.
 * @param key_len Length of the key.
 * @param callback A Function pointer to a callback called for each value
 * @param user_data Pointer to user data passed for callback calls.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_get_array(litestore* ctx,
                        const char* key, const size_t key_len,
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
 * @param key_len Length of the key, excluding null terminator.
 * @param data Iterator for the array data.
 * @return LITESTORE_OK on success
 *         LITESTORE_ERR on error.
 */
int litestore_update_array(litestore* ctx,
                           const char* key, const size_t key_len,
                           litestore_array_iterator data);
/**
 * Save 'key-value'-object in the store.
 * Associate all key-value pairs described by 'data' iterator
 * to the given key.
 * Will fail if key exists.
 *
 * @param ctx
 * @param key The key.
 * @param key_len Length of the key.
 * @param data Iterator for the key-value data.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_save_kv(litestore* ctx,
                      const char* key, const size_t key_len,
                      litestore_kv_iterator data);
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
typedef int (*litestore_get_kv_cb)(
    const char* key, const size_t key_len,
    const void* kv_key, const size_t kv_key_len,
    const void* kv_value, const size_t kv_value_len,
    void* user_data);
/**
 * Get a 'key-value'-object with the given key.
 * The given callback will be called for each value kv pair.
 *
 * @param ctx
 * @param key The key.
 * @param key_len Length of the key.
 * @param callback A Function pointer to a callbeck called for each
 *                 value pair.
 * @param user_data Pointer to user data passed for callback calls.
 * @return LITESTORE_OK on success,
 *         LITESTORE_ERR otherwise.
 */
int litestore_get_kv(litestore* ctx,
                     const char* key, const size_t key_len,
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
 * @param key_len Length of the key, excluding null terminator.
 * @param data Iterator for the key-value data.
 * @return LITESTORE_OK on success
 *         LITESTORE_ERR on error.
 */
int litestore_update_kv(litestore* ctx,
                        const char* key, const size_t key_len,
                        litestore_kv_iterator data);
/**
 * Delete the given entry from the store.
 * Deletes all types.
 *
 * @return LITESTORE_OK on success,
 *         LITESTORE_UNKNOWN_ENTITY if key is not found,
 *         LITESTORE_ERR on other error.
 */
int litestore_delete(litestore* ctx,
                     const char* key, const size_t key_len);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* LITESTORE_LITESTORE_H */
