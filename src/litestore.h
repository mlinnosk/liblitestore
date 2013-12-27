#ifndef LITESTORE_LITESTORE_H
#define LITESTORE_LITESTORE_H

#include <stddef.h>


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
 * Get 'null' value.
 * Checks for existance, since value is null.
 *
 * @param ctx
 * @param key The key.
 * @param key_len Length of the key.
 * @return LITESTORE_OK on success,
 *         LITESTORE_UNKNOWN_ENTITY if the objects is not found,
 *         LITESTORE_ERR otherwise.
 */
int litestore_get_null(litestore* ctx,
                       const char* key, const size_t key_len);
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
int litestore_put_null(litestore* ctx,
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
 * Get a 'raw' value with the given key.
 *
 * @param ctx
 * @param key The key.
 * @param key_len Length of the key.
 * @param value A pointer to a pointer for the value.
 *              This function will allocate a new string for the
 *              value if one is found in the store.
 *              The lenght of the data, not including NULL terminator,
 *              will be placed in the value_len parameter.
 * @param value_len If data type is other than LS_NULL,
 *                  On success will contain the length of the data,
 *                  NOT including NULL terminator.
 *
 * @return LITESTORE_OK on success,
 *         LITESTORE_UNKNOWN_ENTITY if the objects is not found,
 *         LITESTORE_ERR otherwise.
 */
int litestore_get_raw(litestore* ctx,
                      const char* key, const size_t key_len,
                      char** value, size_t* value_len);

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
int litestore_put_raw(litestore* ctx,
                      const char* key, const size_t key_len,
                      const char* value, const size_t value_len);

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
                         const char* value, const size_t value_len);

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
