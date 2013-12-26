#include "litestore_data_parser.h"

#include <ctype.h>

#include "litestore.h"

#define VALUE_SEPARATOR ','
#define KV_SEPATATOR ':'
#define OPEN_ARR '['
#define CLOSE_ARR ']'
#define OPEN_OBJ '{'
#define CLOSE_OBJ '}'

const char* trim(const char* data, size_t* data_len)
{
    size_t len = *data_len;
    while (isspace(*data) && len > 0)
    {
        ++data;
        --len;
    }
    const char* end = data + len - 1;
    while (isspace(*end) && len > 0)
    {
        --end;
        --len;
    }
    *data_len = len;
    return data;
}

int trim_and_call(const char* data, size_t data_len,
                  litestore_parser_ctx* ctx)
{
    data = trim(data, &data_len);
    if (data_len > 0 && ctx->array_cb)
    {
        (*ctx->array_cb)(data, data_len, ctx->user_data);
        return LITESTORE_OK;
    }
    return LITESTORE_ERR;
}

int parse_array(const char* data, const size_t data_len,
                litestore_parser_ctx* ctx)
{
    /* skip decorations */
    const size_t END = data_len - 1;
    size_t pos = 1;
    size_t beg = 1;
    int parsing = 0;

    while (pos < END)
    {
        if (parsing || !isspace(data[pos]))
        {
            parsing = 1;
            if (data[pos] == VALUE_SEPARATOR)
            {
                if (pos > beg)
                {
                    if (trim_and_call(data + beg, pos - beg, ctx)
                        != LITESTORE_OK)
                    {
                        return LITESTORE_ERR;
                    }
                    beg = pos + 1;
                    parsing = 0;
                }
            }
        }
        ++pos;
    }
    if (pos > beg)
    {
        if (trim_and_call(data + beg, pos - beg, ctx)
            != LITESTORE_OK)
        {
            return LITESTORE_ERR;
        }
    }

    return LITESTORE_OK;
}

typedef struct
{
    litestore_dp_obj_cb obj_cb;
    void* user_data;
    int rv;
} obj_ctx_t;

/* @note the data is trimmed when this is called */
void dp_obj_parse_cb(const char* data, const size_t data_len,
                     void* user_data)
{
    obj_ctx_t* ctx = (obj_ctx_t*)user_data;

    /* find the separator */
    const char* key = data;
    size_t key_len = 0;
    while (*data != KV_SEPATATOR && key_len < data_len)
    {
        ++data;
        ++key_len;
    }
    if (key_len < data_len)
    {
        /* value right of separator */
        const char* value = data + 1;
        /* ptr diff */
        size_t value_len = ((key + data_len) - value) * sizeof(char);

        /* trim */
        key = trim(key, &key_len);
        value = trim(value, &value_len);

        if (key_len > 0 && value_len > 0 && ctx->obj_cb)
        {
            (*ctx->obj_cb)(key, key_len, value, value_len, ctx->user_data);
        }
        else
        {
            ctx->rv = LITESTORE_ERR;
        }
    }
    else
    {
        ctx->rv = LITESTORE_ERR;
    }
}

int parse_obj(const char* data, const size_t data_len,
              litestore_parser_ctx* ctx)
{
    int rv = LITESTORE_ERR;

    obj_ctx_t obj_ctx = {ctx->obj_cb, ctx->user_data, LITESTORE_OK};
    litestore_parser_ctx tmp_ctx = {&dp_obj_parse_cb, NULL, &obj_ctx};

    rv = parse_array(data, data_len, &tmp_ctx);
    if (rv == LITESTORE_OK)
    {
        rv = obj_ctx.rv;
    }

    return rv;
}

/**
 * Check if [beg, end[ has onlu white spaces.
 * @return true / false
 */
int is_empty_range(const char* beg, const char* end)
{
    while (beg != end)
    {
        if (!isspace(*beg))
        {
            return 0;
        }
        ++beg;
    }
    return 1;
}

int litestore_data_parse_get_type(const char* data, const size_t data_len,
                                  int* type)
{
    int rv = LITESTORE_ERR;

    if (data && data_len > 0)
    {
        const char f = *data;
        const char e = data[data_len - 1];
        if (f == OPEN_ARR && e == CLOSE_ARR)
        {
            if (is_empty_range(data + 1, data + data_len - 1))
            {
                *type = LS_DP_EMPTY_ARRAY;
            }
            else
            {
                *type = LS_DP_ARRAY;
            }
            rv = LITESTORE_OK;
        }
        if (f == OPEN_OBJ && e == CLOSE_OBJ)
        {
            if (is_empty_range(data + 1, data + data_len - 1))
            {
                *type = LS_DP_EMPTY_OBJ;
            }
            else
            {
                *type = LS_DP_OBJ;
            }
            rv = LITESTORE_OK;
        }
    }
    return rv;
}

int litestore_data_parse(const char* data, const size_t data_len,
                         litestore_parser_ctx ctx)
{
    int rv = LITESTORE_ERR;

    if (data && data_len > 0)
    {
        int type = -1;
        litestore_data_parse_get_type(data, data_len, &type);
        if (type == LS_DP_ARRAY)
        {
            rv = parse_array(data, data_len, &ctx);
        }
        else if (type == LS_DP_OBJ)
        {
            rv = parse_obj(data, data_len, &ctx);
        }
    }

    return rv;
}
