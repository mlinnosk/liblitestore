#ifndef LITESTORE_HELPERS_H
#define LITESTORE_HELPERS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Type for 'blob' data.
 * This is a handle-object that is used in the API
 * for unifying it.
 */
typedef struct
{
    const void* data;
    const size_t size;
} litestore_blob_t;

/**
 * Create blob.
 * Will _NOT_ allocate new storage for data, I.E. the pointer
 * must be valid during the usage of the blob object.
 *
 * @param data Pointer to the data.
 * @param size Size of the data.
 * @return A blob object.
 */
litestore_blob_t litestore_make_blob(const void* data, const size_t size);

/**
 * Type for string 'slice'.
 * A slice is a part of a string that does not have to be null terminated.
 * Terminology borrowed from the 'Go' language.
 * http://blog.golang.org/slices
 */
typedef struct
{
    const char* data;
    const size_t length;
} litestore_slice_t;

/**
 * Create a slice.
 * Slice the given string.
 * Will _NOT_ allocate storeage.
 *
 * @param str The string.
 * @param length The length of the slice.
 * @return A slice.
 */
litestore_slice_t litestore_slice(const char* str, const size_t length);

#ifdef __cplusplus
}  /* extern "C" */
#endif
#endif  /* LITESTORE_HELPERS_H */
