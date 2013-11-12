#ifndef LITESTORE_LITESTORE_H
#define LITESTORE_LITESTORE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct litestore_ctx litestore_ctx;

enum
{
    LITESTORE_OK = 0,
    LITESTORE_ERR = -1
};

int litestore_open(const char* db_file_name, litestore_ctx** ctx);
void litestore_close(litestore_ctx* ctx);

int litestore_get(litestore_ctx* ctx, const char* key, char** value);
int litestore_save(litestore_ctx* ctx,
                   const char* key, const char* value);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* LITESTORE_LITESTORE_H */
