#include "litestore.h"

#include <stdlib.h>

#ifdef __cplusplus
extern "C"
#endif

struct LiteStoreCtx
{
    int i;
};

LiteStoreCtx* litestore_open(const char* db_file_name)
{
    return (LiteStoreCtx*)malloc(sizeof(LiteStoreCtx));
}

void litestore_close(LiteStoreCtx* ctx)
{
    free(ctx);
}

char* litestore_get(LiteStoreCtx* ctx, const char* key)
{
    return NULL;
}

int litestore_save(LiteStoreCtx* ctx,
                   const char* key, const char* value)
{
    return 0;
}

#ifdef __cplusplus
}  // extern "C"
#endif
