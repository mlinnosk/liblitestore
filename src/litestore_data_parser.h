#ifndef LITESTORE_DATA_PARSER_H
#define LITESTORE_DATA_PARSER_H

#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callback type for array data.
 *
 * @param data
 * @param data_len
 * @param obj User data
 */
typedef void (*litestore_dp_array_cb)(const char* data,
                                      const size_t data_len,
                                      void* user_data);
/**
 * Callback type for object data.
 * @param key
 * @param key_len
 * @param data
 * @param data_len
 * @param obj User data
 */
typedef void (*litestore_dp_obj_cb)(
    const char* key, const size_t key_len,
    const char* data, const size_t data_len,
    void* user_data);

/**
 * Struct to wrap the callbakcs and user data.
 * User is responsible for memory management,
 * pointers will not be stores, only used during parsing.
 */
typedef struct
{
    litestore_dp_array_cb array_cb;
    litestore_dp_obj_cb obj_cb;
    void* user_data;
} litestore_parser_ctx;


/**
 * Parser function for data such as Json.
 *
 * @param data The data as UTF-8 string.
 * @param data_len The length of data.
 * @param ctx The parsing context.
 * @return LITESTORE_OK on success.
 *         LITESTORE_ERR if data can't be parserd.
 */
int litestore_data_parse(const char* data, const size_t data_len,
                         litestore_parser_ctx ctx);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* LITESTORE_DATA_PARSER_H */
