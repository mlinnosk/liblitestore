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
 * Get a value with the given key.
 * Operation will depend on the actual data type.
 *
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
int litestore_get(litestore* ctx,
                  const char* key, const size_t key_len,
                  char** value, size_t* value_len);

/**
 * Associate the given (new) key with the given value.
 * Will fail if key exists.
 *
 * Will deduce the type of the data.
 * @see litestore_put_raw
 *
 * @param key The key.
 * @param key_len Length of the key, excluding null terminator.
 * @param value The value, or NULL.
 * @param value_len Length of the value in bytes, or 0.
 * @return LITESTORE_OK on success
 *         LITESTORE_ERR on error.
 */
int litestore_put(litestore* ctx,
                  const char* key, const size_t key_len,
                  const char* value, const size_t value_len);

/**
 * Associate the given (new) key with the given value.
 * Will fail if key exists.
 * Will force the type to RAW. This can have better performance
 * in some cases.
 *
 * This version will NOT accept NULL data.
 *
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
 * Update existing value with new data.
 * If the key does not exist, it will be created.
 *
 * @param key The key.
 * @param key_len Length of the key, excluding null terminator.
 * @param value The value.
 * @param value_len Length of the value in bytes.
 * @return LITESTORE_OK on success
 *         LITESTORE_ERR on error.
 */
int litestore_update(litestore* ctx,
                     const char* key, const size_t key_len,
                     const char* value, const size_t value_len);

/**
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
