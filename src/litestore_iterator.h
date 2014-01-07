#ifndef LITESTORE_ITERATOR_H
#define LITESTORE_ITERATOR_H


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Interface for iterating string based key-value- (map) and
 * array-objects.
 * Users needs to implement the needed functions and create
 * the apropriate iterator object.
 *
 * Example for std::map and std::vector can be found in
 * tests/litestore_iterator_test.cpp.
 */

/**
 * Begin iteration.
 */
typedef void (*litestore_iter_begin)(void* user_data);

/**
 * @return 1 If given iterator has finihed.
 */
typedef int (*litestore_iter_end)(void* user_data);

/**
 * Move iterator to next value.
 */
typedef void (*litestore_iter_next)(void* user_data);

/**
 * Get key-value pair.
 */
typedef void (*litestore_iter_kv_value)
(void* user_data,
 const void** key, size_t* key_len,
 const void** value, size_t* value_len);

/**
 * Structure for key-value iterator object.
 */
typedef struct
{
    void* user_data;

    litestore_iter_begin begin;
    litestore_iter_end end;
    litestore_iter_next next;
    litestore_iter_kv_value value;
} litestore_kv_iterator;

/**
 * Get value.
 */
typedef void (*litestore_iter_array_value)
(void* user_data, const void** value, size_t* value_len);

/**
 * Structure for array iterator object.
 */
typedef struct
{
    void* user_data;

    litestore_iter_begin begin;
    litestore_iter_end end;
    litestore_iter_next next;
    litestore_iter_array_value value;
} litestore_array_iterator;


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* LITESTORE_ITERATOR_H */
