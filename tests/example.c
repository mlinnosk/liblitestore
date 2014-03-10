#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "litestore.h"

/* Callback for raw data, that allocates a new string */
static
int alloc_str(litestore_blob_t value, void* user_data)
{
    char** res = (char**)user_data;
    *res = strndup((const char*)value.data, value.size);
    return LITESTORE_OK;
}

static
void err(const int error_code, const char* error_str, void* user_data)
{
    printf("ERR: (%d): %s\n", error_code, error_str);
}

int main(const int argc, const char** argv)
{
    if (argc < 2)
    {
        printf("Must provide DB file name!\n");
        return 0;
    }
    else
    {
        printf("Opening DB to '%s'.\n", argv[1]);
    }

    /* Open the connection */
    litestore* ls = NULL;
    litestore_opts opts = {&err, NULL};
    if (litestore_open(argv[1], opts, &ls) != LITESTORE_OK)
    {
        litestore_close(ls);
        printf("ERROR 1!\n");
        return -1;
    }

    /* Save raw data */
    const char* key = "Hello";
    const char* value = "World!";
    if (litestore_create_raw(ls,
                             litestore_slice_str(key),
                             litestore_make_blob(value, strlen(value)))
        != LITESTORE_OK)
    {
        litestore_close(ls);
        printf("ERROR 2!\n");
        return -1;
    }

    /* Read the created data, note callback usage */
    char* res = NULL;
    if (litestore_read_raw(ls,
                           litestore_slice_str(key),
                           &alloc_str, &res)
        == LITESTORE_OK)
    {
        printf("%s %s\n", key, res);
        free(res);
    }
    else
    {
        printf("ERROR 3!\n");
    }

    /* Clean up, close the connection */
    litestore_close(ls);
    return 0;
}
