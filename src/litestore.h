#ifndef LITESTORE_LITESTORE_H
#define LITESTORE_LITESTORE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LiteStoreCtx LiteStoreCtx;

LiteStoreCtx* litestore_open(const char* db_file_name);
void litestore_close(LiteStoreCtx* ctx);

char* litestore_get(LiteStoreCtx* ctx, const char* key);
int litestore_save(LiteStoreCtx* ctx,
                   const char* key, const char* value);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* LITESTORE_LITESTORE_H */
