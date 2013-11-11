#ifndef LITESTORE_LITESTORE_H
#define LITESTORE_LITESTORE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct litestore_ctx litestore_ctx;

litestore_ctx* litestore_open(const char* db_file_name);
void litestore_close(litestore_ctx* ctx);

char* litestore_get(litestore_ctx* ctx, const char* key);
int litestore_save(litestore_ctx* ctx,
                   const char* key, const char* value);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* LITESTORE_LITESTORE_H */
